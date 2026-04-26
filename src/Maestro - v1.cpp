#include "plugin.hpp"
#include <random>
#include <cmath>

struct Maestro : Module {
    enum ParamId {
        NUM_CH_PARAM,
        DENSITY_PARAM,
        DENSITY_ATTV_PARAM,
        RESTRICT_PARAM,
        EVAL_EVERY_PARAM,
        FADE_IN_PARAM,
        FADE_OUT_PARAM,
        PROB_PARAM_1, PROB_PARAM_2, PROB_PARAM_3,
        PROB_PARAM_4, PROB_PARAM_5, PROB_PARAM_6,
        PROB_ATTV_PARAM_1, PROB_ATTV_PARAM_2, PROB_ATTV_PARAM_3,
        PROB_ATTV_PARAM_4, PROB_ATTV_PARAM_5, PROB_ATTV_PARAM_6,
        FADE_SWITCH_1, FADE_SWITCH_2, FADE_SWITCH_3,
        FADE_SWITCH_4, FADE_SWITCH_5, FADE_SWITCH_6,
        PARAMS_LEN
    };
    enum InputId {
        CLK_INPUT,
        DENSITY_INPUT,
        PROB_INPUT_1, PROB_INPUT_2, PROB_INPUT_3,
        PROB_INPUT_4, PROB_INPUT_5, PROB_INPUT_6,
        CH_INPUT_1, CH_INPUT_2, CH_INPUT_3,
        CH_INPUT_4, CH_INPUT_5, CH_INPUT_6,
        INPUTS_LEN
    };
    enum OutputId {
        CH_OUTPUT_1, CH_OUTPUT_2, CH_OUTPUT_3,
        CH_OUTPUT_4, CH_OUTPUT_5, CH_OUTPUT_6,
        ACTIVE_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        CH_LIGHT_1G, CH_LIGHT_1R,
        CH_LIGHT_2G, CH_LIGHT_2R,
        CH_LIGHT_3G, CH_LIGHT_3R,
        CH_LIGHT_4G, CH_LIGHT_4R,
        CH_LIGHT_5G, CH_LIGHT_5R,
        CH_LIGHT_6G, CH_LIGHT_6R,
        LIGHTS_LEN
    };

    static const int MAX_CH = 6;
    bool channelOpen[MAX_CH];
    float fadeLevel[MAX_CH];
    bool inFade[MAX_CH];
    bool lastClk;
    int beatCount;
    std::mt19937 rng;

    Maestro() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        configParam(NUM_CH_PARAM, 1.f, 6.f, 6.f, "Num channels");
        configParam(DENSITY_PARAM, 0.f, 6.f, 3.f, "Density", " voices");
        configParam(DENSITY_ATTV_PARAM, -1.f, 1.f, 0.f, "Density CV attenuverter");
        configParam(RESTRICT_PARAM, 0.f, 1.f, 0.5f, "Restrict");
        configParam(EVAL_EVERY_PARAM, 0.f, 4.f, 1.f, "Evaluate every");
        configParam(FADE_IN_PARAM, 0.01f, 0.5f, 0.05f, "Fade In", " s");
        configParam(FADE_OUT_PARAM, 0.01f, 0.5f, 0.1f, "Fade Out", " s");

        for (int i = 0; i < MAX_CH; i++) {
            configParam(PROB_PARAM_1 + i, 0.f, 1.f, 0.5f, "Probability");
            configParam(PROB_ATTV_PARAM_1 + i, -1.f, 1.f, 0.f, "Prob CV attenuverter");
            configSwitch(FADE_SWITCH_1 + i, 0.f, 1.f, 0.f, "Mode", {"Gate", "Fade"});
            configInput(CH_INPUT_1 + i, "Channel");
            configInput(PROB_INPUT_1 + i, "Prob CV");
            configOutput(CH_OUTPUT_1 + i, "Channel");
            channelOpen[i] = false;
            fadeLevel[i] = 0.f;
            inFade[i] = false;
        }

        configInput(CLK_INPUT, "Clock");
        configInput(DENSITY_INPUT, "Density CV");
        configOutput(ACTIVE_OUTPUT, "Active voices CV");

        lastClk = false;
        beatCount = 0;
        rng.seed(std::random_device{}());
    }

    void evaluate(int numChannels, float densityTarget, float restrict_) {
        int targetVoices;
        if (restrict_ >= 0.999f) {
            targetVoices = (int)rack::math::clamp(std::round(densityTarget), 0.f, (float)numChannels);
        } else {
            float sigma = (1.f - restrict_) * numChannels * 0.5f;
            std::normal_distribution<float> dist(densityTarget, sigma);
            float sampled = dist(rng);
            targetVoices = (int)rack::math::clamp(std::round(sampled), 0.f, (float)numChannels);
        }

        float weights[MAX_CH] = {};
        for (int i = 0; i < numChannels; i++) {
            float knob = params[PROB_PARAM_1 + i].getValue();
            float attv = params[PROB_ATTV_PARAM_1 + i].getValue();
            float cv = inputs[PROB_INPUT_1 + i].isConnected() ?
                       inputs[PROB_INPUT_1 + i].getVoltage() / 10.f * attv : 0.f;
            weights[i] = rack::math::clamp(knob + cv, 0.f, 1.f);
        }

        bool selected[MAX_CH] = {};
        for (int pick = 0; pick < targetVoices; pick++) {
            float totalWeight = 0.f;
            for (int i = 0; i < numChannels; i++) {
                if (!selected[i]) totalWeight += weights[i];
            }
            if (totalWeight <= 0.f) break;

            std::uniform_real_distribution<float> dist(0.f, totalWeight);
            float r = dist(rng);
            float acc = 0.f;
            for (int i = 0; i < numChannels; i++) {
                if (!selected[i]) {
                    acc += weights[i];
                    if (r <= acc) {
                        selected[i] = true;
                        break;
                    }
                }
            }
        }

        for (int i = 0; i < MAX_CH; i++) {
            channelOpen[i] = (i < numChannels) ? selected[i] : false;
            inFade[i] = true;
        }
    }

    void process(const ProcessArgs& args) override {
        bool clk = inputs[CLK_INPUT].getVoltage() > 1.f;
        bool clkRise = clk && !lastClk;
        lastClk = clk;

        int numChannels = (int)rack::math::clamp(
            std::round(params[NUM_CH_PARAM].getValue()), 1.f, 6.f);

        if (clkRise) {
            beatCount++;
            int evalEvery = (int)std::pow(2.f, params[EVAL_EVERY_PARAM].getValue());
            if (beatCount >= evalEvery) {
                beatCount = 0;
                float densityKnob = params[DENSITY_PARAM].getValue();
                float attv = params[DENSITY_ATTV_PARAM].getValue();
                float densityCV = inputs[DENSITY_INPUT].isConnected() ?
                                  inputs[DENSITY_INPUT].getVoltage() / 10.f * 6.f * attv : 0.f;
                float densityTarget = rack::math::clamp(densityKnob + densityCV, 0.f, (float)numChannels);
                float restrict_ = params[RESTRICT_PARAM].getValue();
                evaluate(numChannels, densityTarget, restrict_);
            }
        }

        float fadeInTime  = params[FADE_IN_PARAM].getValue();
        float fadeOutTime = params[FADE_OUT_PARAM].getValue();
        int activeCount = 0;

        for (int i = 0; i < MAX_CH; i++) {
            bool fadeMode = params[FADE_SWITCH_1 + i].getValue() > 0.5f;

            if (inFade[i]) {
                if (fadeMode) {
                    if (channelOpen[i]) {
                        fadeLevel[i] += args.sampleTime / fadeInTime;
                    } else {
                        fadeLevel[i] -= args.sampleTime / fadeOutTime;
                    }
                    fadeLevel[i] = rack::math::clamp(fadeLevel[i], 0.f, 1.f);
                    if (fadeLevel[i] <= 0.f || fadeLevel[i] >= 1.f) {
                        inFade[i] = false;
                    }
                } else {
                    fadeLevel[i] = channelOpen[i] ? 1.f : 0.f;
                    inFade[i] = false;
                }
            }

            float inSignal = inputs[CH_INPUT_1 + i].isConnected() ?
                             inputs[CH_INPUT_1 + i].getVoltage() : 1.f;

            outputs[CH_OUTPUT_1 + i].setVoltage(inSignal * fadeLevel[i]);

            int lightBase = i * 2;
            if (channelOpen[i] && !inFade[i]) {
                lights[lightBase].setBrightness(1.f);
                lights[lightBase + 1].setBrightness(0.f);
                activeCount++;
            } else if (inFade[i] && fadeMode) {
                lights[lightBase].setBrightness(0.8f);
                lights[lightBase + 1].setBrightness(0.5f);
            } else {
                lights[lightBase].setBrightness(0.f);
                lights[lightBase + 1].setBrightness(0.f);
            }
        }

        outputs[ACTIVE_OUTPUT].setVoltage((float)activeCount / MAX_CH * 10.f);
    }
};

struct MaestroWidget : ModuleWidget {
    MaestroWidget(Maestro* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Maestro.svg")));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Fila 1: globals
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8, 15)), module, Maestro::CLK_INPUT));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(20, 15)), module, Maestro::NUM_CH_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(32, 15)), module, Maestro::DENSITY_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(44, 15)), module, Maestro::DENSITY_ATTV_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(56, 15)), module, Maestro::DENSITY_INPUT));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(68, 15)), module, Maestro::RESTRICT_PARAM));

        // Fila 2: eval + fades + active out
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(8, 28)), module, Maestro::EVAL_EVERY_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(22, 28)), module, Maestro::FADE_IN_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(36, 28)), module, Maestro::FADE_OUT_PARAM));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(68, 28)), module, Maestro::ACTIVE_OUTPUT));

        // Canals 1-6
        for (int i = 0; i < 6; i++) {
            float y = 45.f + i * 15.f;
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8, y)), module, Maestro::CH_INPUT_1 + i));
            addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(20, y)), module, Maestro::PROB_PARAM_1 + i));
            addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(32, y)), module, Maestro::PROB_ATTV_PARAM_1 + i));
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(44, y)), module, Maestro::PROB_INPUT_1 + i));
            addParam(createParamCentered<CKSS>(mm2px(Vec(54, y)), module, Maestro::FADE_SWITCH_1 + i));
            addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(62, y)), module, Maestro::CH_LIGHT_1G + i * 2));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(72, y)), module, Maestro::CH_OUTPUT_1 + i));
        }
    }
};

Model* modelMaestro = createModel<Maestro, MaestroWidget>("Maestro");
