#include "plugin.hpp"
#include <random>
#include <cmath>

struct Maestro : Module {
    enum ParamId {
        DENSITY_PARAM,
        EVAL_EVERY_PARAM,
        FADE_IN_PARAM,
        FADE_OUT_PARAM,
        PROB_PARAM_1, PROB_PARAM_2, PROB_PARAM_3,
        PROB_PARAM_4, PROB_PARAM_5, PROB_PARAM_6,
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
        CH_LIGHT_1, CH_LIGHT_2, CH_LIGHT_3,
        CH_LIGHT_4, CH_LIGHT_5, CH_LIGHT_6,
        LIGHTS_LEN
    };

    static const int NUM_CH = 6;
    bool channelOpen[NUM_CH];
    float fadeLevel[NUM_CH];
    bool inFade[NUM_CH];
    bool lastClk;
    int beatCount;
    std::mt19937 rng;

    Maestro() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam(DENSITY_PARAM, 0.f, 6.f, 3.f, "Density", " voices");
        configParam(EVAL_EVERY_PARAM, 0.f, 4.f, 1.f, "Evaluate every");
        configParam(FADE_IN_PARAM, 0.01f, 0.5f, 0.05f, "Fade In", " s");
        configParam(FADE_OUT_PARAM, 0.01f, 0.5f, 0.1f, "Fade Out", " s");
        for (int i = 0; i < NUM_CH; i++) {
            configParam(PROB_PARAM_1 + i, 0.f, 1.f, 0.5f, "Probability");
            configInput(CH_INPUT_1 + i, "Channel");
            configInput(PROB_INPUT_1 + i, "Prob CV");
            configOutput(CH_OUTPUT_1 + i, "Channel");
            configLight(CH_LIGHT_1 + i, "Channel active");
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

    void evaluate(int targetVoices) {
        float weights[NUM_CH];
        for (int i = 0; i < NUM_CH; i++) {
            float knob = params[PROB_PARAM_1 + i].getValue();
            float cv = inputs[PROB_INPUT_1 + i].isConnected() ?
                       inputs[PROB_INPUT_1 + i].getVoltage() / 10.f : 0.f;
            weights[i] = rack::math::clamp(knob + cv, 0.f, 1.f);
        }

        bool selected[NUM_CH] = {};
        int remaining = rack::math::clamp(targetVoices, 0, NUM_CH);

        for (int pick = 0; pick < remaining; pick++) {
            float totalWeight = 0.f;
            for (int i = 0; i < NUM_CH; i++) {
                if (!selected[i]) totalWeight += weights[i];
            }
            if (totalWeight <= 0.f) break;

            std::uniform_real_distribution<float> dist(0.f, totalWeight);
            float r = dist(rng);
            float acc = 0.f;
            for (int i = 0; i < NUM_CH; i++) {
                if (!selected[i]) {
                    acc += weights[i];
                    if (r <= acc) {
                        selected[i] = true;
                        break;
                    }
                }
            }
        }

        for (int i = 0; i < NUM_CH; i++) {
            channelOpen[i] = selected[i];
            inFade[i] = true;
        }
    }

    void process(const ProcessArgs& args) override {
        bool clk = inputs[CLK_INPUT].getVoltage() > 1.f;
        bool clkRise = clk && !lastClk;
        lastClk = clk;

        if (clkRise) {
            beatCount++;
            int evalEvery = (int)std::pow(2.f, params[EVAL_EVERY_PARAM].getValue());
            if (beatCount >= evalEvery) {
                beatCount = 0;
                float densityKnob = params[DENSITY_PARAM].getValue();
                float densityCV = inputs[DENSITY_INPUT].isConnected() ?
                                  inputs[DENSITY_INPUT].getVoltage() / 10.f * 6.f : 0.f;
                int targetVoices = (int)rack::math::clamp(densityKnob + densityCV, 0.f, 6.f);
                evaluate(targetVoices);
            }
        }

        float fadeInTime  = params[FADE_IN_PARAM].getValue();
        float fadeOutTime = params[FADE_OUT_PARAM].getValue();
        int activeCount = 0;

        for (int i = 0; i < NUM_CH; i++) {
            if (inFade[i]) {
                if (channelOpen[i]) {
                    fadeLevel[i] += args.sampleTime / fadeInTime;
                } else {
                    fadeLevel[i] -= args.sampleTime / fadeOutTime;
                }
                fadeLevel[i] = rack::math::clamp(fadeLevel[i], 0.f, 1.f);
                if (fadeLevel[i] <= 0.f || fadeLevel[i] >= 1.f) {
                    inFade[i] = false;
                }
            }

            float inSignal = inputs[CH_INPUT_1 + i].getVoltage();
            outputs[CH_OUTPUT_1 + i].setVoltage(inSignal * fadeLevel[i]);

            if (channelOpen[i] && !inFade[i]) {
                lights[CH_LIGHT_1 + i].setBrightness(1.f);
                activeCount++;
            } else if (inFade[i]) {
                lights[CH_LIGHT_1 + i].setBrightness(0.5f);
            } else {
                lights[CH_LIGHT_1 + i].setBrightness(0.f);
            }
        }

        outputs[ACTIVE_OUTPUT].setVoltage((float)activeCount / NUM_CH * 10.f);
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

        // Globals
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8, 15)), module, Maestro::CLK_INPUT));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22, 15)), module, Maestro::DENSITY_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(36, 15)), module, Maestro::DENSITY_INPUT));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(50, 15)), module, Maestro::EVAL_EVERY_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(36, 28)), module, Maestro::FADE_IN_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(50, 28)), module, Maestro::FADE_OUT_PARAM));

        // Canals 1-6
        for (int i = 0; i < 6; i++) {
            float y = 45.f + i * 15.f;
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8, y)), module, Maestro::CH_INPUT_1 + i));
            addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(22, y)), module, Maestro::PROB_PARAM_1 + i));
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(36, y)), module, Maestro::PROB_INPUT_1 + i));
            addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(50, y)), module, Maestro::CH_LIGHT_1 + i));
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(64, y)), module, Maestro::CH_OUTPUT_1 + i));
        }

        // Sortida global
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(8, 28)), module, Maestro::ACTIVE_OUTPUT));
    }
};

Model* modelMaestro = createModel<Maestro, MaestroWidget>("Maestro");