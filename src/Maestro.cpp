#include "plugin.hpp"
#include <random>
#include <cmath>

// ParamQuantity personalitzat per mostrar 1,2,4,8,16 al LENGTH
struct BarsParamQuantity : ParamQuantity {
    std::string getDisplayValueString() override {
        int idx = (int)rack::math::clamp(std::round(getValue()), 0.f, 4.f);
        int bars = (int)std::pow(2.f, idx);
        return std::to_string(bars);
    }
    std::string getUnit() override {
        return " bar(s)";
    }
};

struct Maestro;

struct DensityParamQuantity : ParamQuantity {
    float getMinValue() override;
    float getMaxValue() override;
    std::string getDisplayValueString() override {
        return std::to_string((int)std::round(getValue()));
    }
};

// Jack de sortida amb indicador de voltatge
struct VoltageOutputPort : PJ301MPort {
    float* voltagePtr = nullptr;

    void draw(const DrawArgs& args) override {
        PJ301MPort::draw(args);
        if (voltagePtr) {
            float v = rack::math::clamp(std::abs(*voltagePtr) / 10.f, 0.f, 1.f);
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, box.size.x / 2, box.size.y / 2, 3.5f);
            nvgFillColor(args.vg, nvgRGBf(0.f, v * 0.9f, 0.f));
            nvgFill(args.vg);
        }
    }
};

// Camp de text editable per canal
struct ChannelLabel : widget::OpaqueWidget {
    std::string* label = nullptr;
    std::string defaultText = "";
    bool editing = false;
    std::string editBuffer;

    void draw(const DrawArgs& args) override {
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 2);
        nvgFillColor(args.vg, nvgRGB(30, 30, 50));
        nvgFill(args.vg);
        if (editing) {
            nvgStrokeColor(args.vg, nvgRGB(100, 100, 220));
            nvgStrokeWidth(args.vg, 1.f);
            nvgStroke(args.vg);
        }
        nvgFontSize(args.vg, 9.f);
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgFillColor(args.vg, nvgRGB(200, 200, 220));
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        std::string display = editing ? editBuffer + "_" : (label ? *label : defaultText);
        nvgText(args.vg, box.size.x / 2, box.size.y / 2, display.c_str(), NULL);
    }

    void onDoubleClick(const DoubleClickEvent& e) override {
        editing = true;
        editBuffer = label ? *label : "";
        APP->event->setSelectedWidget(this);
        e.consume(this);
    }

    void onDeselect(const DeselectEvent& e) override {
        if (editing) {
            if (label) *label = editBuffer;
            editing = false;
        }
    }

    void onSelectKey(const SelectKeyEvent& e) override {
        if (!editing) return;
        if (e.action == GLFW_PRESS || e.action == GLFW_REPEAT) {
            if (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_ESCAPE) {
                if (e.key == GLFW_KEY_ENTER && label) *label = editBuffer;
                editing = false;
                APP->event->setSelectedWidget(nullptr);
                e.consume(this);
            } else if (e.key == GLFW_KEY_BACKSPACE) {
                if (!editBuffer.empty()) editBuffer.pop_back();
                e.consume(this);
            }
        }
    }

    void onSelectText(const SelectTextEvent& e) override {
        if (!editing) return;
        if (editBuffer.size() < 4) {
            editBuffer += (char)e.codepoint;
        }
        e.consume(this);
    }
};

struct Maestro : Module {
    enum ParamId {
        NUM_CH_PARAM,
        DENSITY_PARAM,
        DENSITY_ATTV_PARAM,
        RESTRICT_PARAM,
        BLOCK_PARAM,
        SKIP_PARAM,
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
        RESET_INPUT,
        DENSITY_INPUT,
        BLOCK_INPUT,
        SKIP_INPUT,
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
        CH_LIGHT_1R, CH_LIGHT_1G, CH_LIGHT_1B,
        CH_LIGHT_2R, CH_LIGHT_2G, CH_LIGHT_2B,
        CH_LIGHT_3R, CH_LIGHT_3G, CH_LIGHT_3B,
        CH_LIGHT_4R, CH_LIGHT_4G, CH_LIGHT_4B,
        CH_LIGHT_5R, CH_LIGHT_5G, CH_LIGHT_5B,
        CH_LIGHT_6R, CH_LIGHT_6G, CH_LIGHT_6B,
        LIGHTS_LEN
    };

    static const int MAX_CH = 6;
    bool channelOpen[MAX_CH];
    float fadeLevel[MAX_CH];
    bool inFade[MAX_CH];
    bool lastClk;
    bool lastReset;
    int beatCount;
    std::mt19937 rng;
    std::string channelLabels[MAX_CH];
    int minVoices = 0;
    int beatsPerBar = 4;
    float chVoltage[MAX_CH];
    float defaultInputVoltage = 1.f;
    bool activeOutProportional = true;

    // Nous parametres v7
    bool skipBinary = false;        // false=probabilistic, true=binary
    bool skipPerChannel = false;    // false=global, true=per canal
    bool resetForceEval = false;    // false=reset bars, true=force evaluate

    // Trigger mode: pols curt per cada canal
    float triggerTimer[MAX_CH];
    float triggerDuration = 0.005f; // 5ms per defecte
    float triggerFlashTimer[MAX_CH]; // timer per LED blanc (300ms)
    static constexpr float TRIGGER_FLASH_DURATION = 0.3f; // 300ms

    Maestro() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        configParam(NUM_CH_PARAM, 1.f, 6.f, 6.f, "Active tracks");
        getParamQuantity(NUM_CH_PARAM)->snapEnabled = true;

        configParam<DensityParamQuantity>(DENSITY_PARAM, 0.f, 6.f, 3.f, "Track density", " voices");
        getParamQuantity(DENSITY_PARAM)->snapEnabled = true;

        configParam(DENSITY_ATTV_PARAM, -1.f, 1.f, 0.f, "Density CV attenuverter");
        configParam(RESTRICT_PARAM, 0.f, 1.f, 0.f, "Randomness");

        configParam<BarsParamQuantity>(BLOCK_PARAM, 0.f, 4.f, 0.f, "Length");
        getParamQuantity(BLOCK_PARAM)->snapEnabled = true;

        configParam(SKIP_PARAM, 0.f, 1.f, 0.f, "Skip probability");
        configParam(FADE_IN_PARAM, 0.f, 10.f, 0.05f, "Fade In", " s");
        configParam(FADE_OUT_PARAM, 0.f, 10.f, 0.1f, "Fade Out", " s");

        for (int i = 0; i < MAX_CH; i++) {
            configParam(PROB_PARAM_1 + i, 0.f, 1.f, 0.5f, "Probability");
            configParam(PROB_ATTV_PARAM_1 + i, -1.f, 1.f, 0.f, "Prob CV attenuverter");
            // T=0, G=1, F=2
            configSwitch(FADE_SWITCH_1 + i, 0.f, 2.f, 1.f, "Mode", {"Trigger", "Gate", "Fade"});
            configInput(CH_INPUT_1 + i, "Channel");
            configInput(PROB_INPUT_1 + i, "Prob CV");
            configOutput(CH_OUTPUT_1 + i, "Channel");
            channelOpen[i] = false;
            fadeLevel[i] = 0.f;
            inFade[i] = false;
            chVoltage[i] = 0.f;
            triggerTimer[i] = 0.f;
            triggerFlashTimer[i] = 0.f;
            channelLabels[i] = "CH" + std::to_string(i + 1);
        }

        configInput(CLK_INPUT, "Clock");
        configInput(RESET_INPUT, "Reset / Force evaluate");
        configInput(DENSITY_INPUT, "Density CV");
        configInput(BLOCK_INPUT, "Length CV");
        configInput(SKIP_INPUT, "Skip CV");
        configOutput(ACTIVE_OUTPUT, "Active voices CV");

        lastClk = false;
        lastReset = false;
        beatCount = 0;
        rng.seed(std::random_device{}());
    }

    int barsValue(float raw) {
        int idx = (int)rack::math::clamp(std::round(raw), 0.f, 4.f);
        return (int)std::pow(2.f, idx);
    }

    void evaluate(int numChannels, float densityTarget, float randomness) {
        float minV = (float)minVoices;
        float maxV = (float)numChannels;
        densityTarget = rack::math::clamp(densityTarget, minV, maxV);

        int targetVoices;
        if (randomness <= 0.001f) {
            targetVoices = (int)std::round(densityTarget);
        } else {
            float sigma = randomness * numChannels * 0.5f;
            std::normal_distribution<float> dist(densityTarget, sigma);
            float sampled = dist(rng);
            targetVoices = (int)rack::math::clamp(std::round(sampled), minV, maxV);
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
            bool newState = (i < numChannels) ? selected[i] : false;
            // Si skipPerChannel, cada canal decideix independentment si es queda
            if (skipPerChannel && i < numChannels) {
                float skipProb = inputs[SKIP_INPUT].isConnected() ?
                                 rack::math::clamp(inputs[SKIP_INPUT].getVoltage() / 10.f, 0.f, 1.f) :
                                 params[SKIP_PARAM].getValue();
                float skipVal = skipBinary ? (skipProb > 0.5f ? 1.f : 0.f) : skipProb;
                std::uniform_real_distribution<float> skipDist(0.f, 1.f);
                if (skipDist(rng) < skipVal) {
                    newState = channelOpen[i]; // conserva l'estat anterior
                }
            }
            channelOpen[i] = newState;
            inFade[i] = true;
            // Mode Trigger: dispara sempre que el canal és seleccionat
            if (newState) {
                int mode = (int)std::round(params[FADE_SWITCH_1 + i].getValue());
                if (mode == 0) {
                    triggerTimer[i] = triggerDuration;
                    triggerFlashTimer[i] = TRIGGER_FLASH_DURATION;
                }
            }
        }
    }

    void process(const ProcessArgs& args) override {
        // RESET / FORCE EVALUATE
        bool reset = inputs[RESET_INPUT].getVoltage() > 1.f;
        bool resetRise = reset && !lastReset;
        lastReset = reset;
        if (resetRise) {
            if (resetForceEval) {
                // Forcar avaluació immediata sense reiniciar el comptador
                int numChannels = (int)rack::math::clamp(
                    std::round(params[NUM_CH_PARAM].getValue()), 1.f, 6.f);
                float densityKnob = params[DENSITY_PARAM].getValue();
                float attv = params[DENSITY_ATTV_PARAM].getValue();
                float densityCV = inputs[DENSITY_INPUT].isConnected() ?
                                  inputs[DENSITY_INPUT].getVoltage() / 10.f * 6.f * attv : 0.f;
                float densityTarget = rack::math::clamp(
                    std::round(densityKnob + densityCV), 0.f, (float)numChannels);
                evaluate(numChannels, densityTarget, params[RESTRICT_PARAM].getValue());
            } else {
                beatCount = 0;
            }
        }

        // CLOCK
        bool clk = inputs[CLK_INPUT].getVoltage() > 1.f;
        bool clkRise = clk && !lastClk;
        lastClk = clk;

        int numChannels = (int)rack::math::clamp(
            std::round(params[NUM_CH_PARAM].getValue()), 1.f, 6.f);

        if (clkRise) {
            beatCount++;
            float barsRaw = inputs[BLOCK_INPUT].isConnected() ?
                            inputs[BLOCK_INPUT].getVoltage() / 10.f * 4.f :
                            params[BLOCK_PARAM].getValue();
            int evalEvery = barsValue(barsRaw) * beatsPerBar;

            if (beatCount >= evalEvery) {
                beatCount = 0;

                // Skip global
                if (!skipPerChannel) {
                    float skipProb = inputs[SKIP_INPUT].isConnected() ?
                                     rack::math::clamp(inputs[SKIP_INPUT].getVoltage() / 10.f, 0.f, 1.f) :
                                     params[SKIP_PARAM].getValue();
                    float skipVal = skipBinary ?
                                    (inputs[SKIP_INPUT].getVoltage() > 1.f ? 1.f : 0.f) :
                                    skipProb;
                    std::uniform_real_distribution<float> skipDist(0.f, 1.f);
                    if (skipVal > 0.f && skipDist(rng) < skipVal) return;
                }

                float densityKnob = params[DENSITY_PARAM].getValue();
                float attv = params[DENSITY_ATTV_PARAM].getValue();
                float densityCV = inputs[DENSITY_INPUT].isConnected() ?
                                  inputs[DENSITY_INPUT].getVoltage() / 10.f * 6.f * attv : 0.f;
                float densityTarget = rack::math::clamp(
                    std::round(densityKnob + densityCV), 0.f, (float)numChannels);
                evaluate(numChannels, densityTarget, params[RESTRICT_PARAM].getValue());
            }
        }

        float fadeInTime  = params[FADE_IN_PARAM].getValue();
        float fadeOutTime = params[FADE_OUT_PARAM].getValue();
        int activeCount = 0;

        for (int i = 0; i < MAX_CH; i++) {
            int mode = (int)std::round(params[FADE_SWITCH_1 + i].getValue());
            // mode: 0=Trigger, 1=Gate, 2=Fade

            if (inFade[i]) {
                if (mode == 2) {
                    // Fade
                    if (channelOpen[i]) {
                        fadeLevel[i] = (fadeInTime > 0.f) ?
                            fadeLevel[i] + args.sampleTime / fadeInTime : 1.f;
                    } else {
                        fadeLevel[i] = (fadeOutTime > 0.f) ?
                            fadeLevel[i] - args.sampleTime / fadeOutTime : 0.f;
                    }
                    fadeLevel[i] = rack::math::clamp(fadeLevel[i], 0.f, 1.f);
                    if (fadeLevel[i] <= 0.f || fadeLevel[i] >= 1.f) {
                        inFade[i] = false;
                    }
                } else {
                    // Gate o Trigger: canvi instantani
                    fadeLevel[i] = channelOpen[i] ? 1.f : 0.f;
                    inFade[i] = false;
                }
            }

            // Mode Trigger: gestionar el timer
            float outputLevel = fadeLevel[i];
            if (mode == 0) {
                if (triggerTimer[i] > 0.f) {
                    triggerTimer[i] -= args.sampleTime;
                    outputLevel = 1.f;
                } else {
                    outputLevel = 0.f;
                }
                if (triggerFlashTimer[i] > 0.f) {
                    triggerFlashTimer[i] -= args.sampleTime;
                }
            }

            // Suport polifonic
            int polyCh = inputs[CH_INPUT_1 + i].isConnected() ?
                         inputs[CH_INPUT_1 + i].getChannels() : 1;
            outputs[CH_OUTPUT_1 + i].setChannels(polyCh);

            float sumVoltage = 0.f;
            for (int c = 0; c < polyCh; c++) {
                float inSignal;
                if (mode == 0 && !inputs[CH_INPUT_1 + i].isConnected()) {
                    // Mode Trigger sense entrada: sempre 10V
                    inSignal = 10.f;
                } else {
                    inSignal = inputs[CH_INPUT_1 + i].isConnected() ?
                               inputs[CH_INPUT_1 + i].getVoltage(c) : defaultInputVoltage;
                }
                float outV = inSignal * outputLevel;
                outputs[CH_OUTPUT_1 + i].setVoltage(outV, c);
                sumVoltage += std::abs(outV);
            }
            chVoltage[i] = sumVoltage / polyCh;

            // LEDs RGB: R=0, G=1, B=2
            int lightBase = i * 3;
            if (mode == 0) {
                // Mode Trigger: blau constant, blanc durant el flash
                if (triggerFlashTimer[i] > 0.f) {
                    // Blanc durant el trigger flash (300ms)
                    lights[lightBase].setBrightness(1.f);   // R
                    lights[lightBase + 1].setBrightness(1.f); // G
                    lights[lightBase + 2].setBrightness(1.f); // B
                } else {
                    // Blau constant
                    lights[lightBase].setBrightness(0.04f);   // R (10/255)
                    lights[lightBase + 1].setBrightness(0.04f); // G (10/255)
                    lights[lightBase + 2].setBrightness(1.f);   // B
                }
                if (channelOpen[i]) activeCount++;
            } else if (channelOpen[i] && !inFade[i]) {
                // Verd: canal obert
                lights[lightBase].setBrightness(0.f);
                lights[lightBase + 1].setBrightness(1.f);
                lights[lightBase + 2].setBrightness(0.f);
                activeCount++;
            } else if (inFade[i] && mode == 2) {
                // Groc: fade en curs
                lights[lightBase].setBrightness(1.f);
                lights[lightBase + 1].setBrightness(1.f);
                lights[lightBase + 2].setBrightness(0.f);
            } else if (!channelOpen[i] && !inFade[i]) {
                // Vermell: canal tancat
                lights[lightBase].setBrightness(1.f);
                lights[lightBase + 1].setBrightness(0.f);
                lights[lightBase + 2].setBrightness(0.f);
            } else {
                lights[lightBase].setBrightness(0.f);
                lights[lightBase + 1].setBrightness(0.f);
                lights[lightBase + 2].setBrightness(0.f);
            }
        }

        // ACTIVE OUTPUT
        float activeCV;
        if (activeOutProportional) {
            activeCV = (numChannels > 0) ?
                       (float)activeCount / (float)numChannels * 10.f : 0.f;
        } else {
            activeCV = (float)activeCount / (float)MAX_CH * 10.f;
        }
        outputs[ACTIVE_OUTPUT].setVoltage(activeCV);
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_t* labelsJ = json_array();
        for (int i = 0; i < MAX_CH; i++) {
            json_array_append_new(labelsJ, json_string(channelLabels[i].c_str()));
        }
        json_object_set_new(rootJ, "channelLabels", labelsJ);
        json_object_set_new(rootJ, "minVoices", json_integer(minVoices));
        json_object_set_new(rootJ, "beatsPerBar", json_integer(beatsPerBar));
        json_object_set_new(rootJ, "defaultInputVoltage", json_real(defaultInputVoltage));
        json_object_set_new(rootJ, "activeOutProportional", json_boolean(activeOutProportional));
        json_object_set_new(rootJ, "skipBinary", json_boolean(skipBinary));
        json_object_set_new(rootJ, "skipPerChannel", json_boolean(skipPerChannel));
        json_object_set_new(rootJ, "resetForceEval", json_boolean(resetForceEval));
        json_object_set_new(rootJ, "triggerDuration", json_real(triggerDuration));
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        json_t* labelsJ = json_object_get(rootJ, "channelLabels");
        if (labelsJ) {
            for (int i = 0; i < MAX_CH; i++) {
                json_t* labelJ = json_array_get(labelsJ, i);
                if (labelJ) channelLabels[i] = json_string_value(labelJ);
            }
        }
        json_t* minVoicesJ = json_object_get(rootJ, "minVoices");
        if (minVoicesJ) minVoices = json_integer_value(minVoicesJ);
        json_t* beatsPerBarJ = json_object_get(rootJ, "beatsPerBar");
        if (beatsPerBarJ) beatsPerBar = json_integer_value(beatsPerBarJ);
        json_t* defaultInputVoltageJ = json_object_get(rootJ, "defaultInputVoltage");
        if (defaultInputVoltageJ) defaultInputVoltage = (float)json_real_value(defaultInputVoltageJ);
        json_t* activeOutProportionalJ = json_object_get(rootJ, "activeOutProportional");
        if (activeOutProportionalJ) activeOutProportional = json_boolean_value(activeOutProportionalJ);
        json_t* skipBinaryJ = json_object_get(rootJ, "skipBinary");
        if (skipBinaryJ) skipBinary = json_boolean_value(skipBinaryJ);
        json_t* skipPerChannelJ = json_object_get(rootJ, "skipPerChannel");
        if (skipPerChannelJ) skipPerChannel = json_boolean_value(skipPerChannelJ);
        json_t* resetForceEvalJ = json_object_get(rootJ, "resetForceEval");
        if (resetForceEvalJ) resetForceEval = json_boolean_value(resetForceEvalJ);
        json_t* triggerDurationJ = json_object_get(rootJ, "triggerDuration");
        if (triggerDurationJ) triggerDuration = (float)json_real_value(triggerDurationJ);
    }
};

// Implementacions de DensityParamQuantity
float DensityParamQuantity::getMinValue() {
    Maestro* m = dynamic_cast<Maestro*>(module);
    if (m) return (float)m->minVoices;
    return 0.f;
}

float DensityParamQuantity::getMaxValue() {
    Maestro* m = dynamic_cast<Maestro*>(module);
    if (m) return rack::math::clamp(
        std::round(m->params[Maestro::NUM_CH_PARAM].getValue()), 1.f, 6.f);
    return 6.f;
}

struct MaestroWidget : ModuleWidget {
    MaestroWidget(Maestro* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Maestro.svg")));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Fila 1
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10, 18)), module, Maestro::CLK_INPUT));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(22, 18)), module, Maestro::NUM_CH_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(34, 18)), module, Maestro::DENSITY_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(46, 18)), module, Maestro::DENSITY_ATTV_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(58, 18)), module, Maestro::DENSITY_INPUT));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(70, 18)), module, Maestro::RESTRICT_PARAM));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(82, 18)), module, Maestro::ACTIVE_OUTPUT));

        // Fila 2
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10, 34)), module, Maestro::RESET_INPUT));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(22, 34)), module, Maestro::BLOCK_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(34, 34)), module, Maestro::BLOCK_INPUT));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(46, 34)), module, Maestro::SKIP_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(58, 34)), module, Maestro::SKIP_INPUT));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(70, 34)), module, Maestro::FADE_IN_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(82, 34)), module, Maestro::FADE_OUT_PARAM));

        // Canals 1-6
        for (int i = 0; i < 6; i++) {
            float y = 51.f + i * 13.f;

            ChannelLabel* label = createWidget<ChannelLabel>(mm2px(Vec(4, y - 4)));
            label->box.size = mm2px(Vec(11, 8));
            if (module) {
                label->label = &module->channelLabels[i];
            } else {
                label->defaultText = "CH" + std::to_string(i + 1);
            }
            addChild(label);

            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22, y)), module, Maestro::CH_INPUT_1 + i));
            addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(33, y)), module, Maestro::PROB_PARAM_1 + i));
            addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(46, y)), module, Maestro::PROB_ATTV_PARAM_1 + i));
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(58, y)), module, Maestro::PROB_INPUT_1 + i));
            // Switch T/G/F (3 posicions)
            addParam(createParamCentered<CKSSThree>(mm2px(Vec(70, y)), module, Maestro::FADE_SWITCH_1 + i));
            addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(75, y)), module, Maestro::CH_LIGHT_1R + i * 3));

            VoltageOutputPort* outPort = createOutputCentered<VoltageOutputPort>(
                mm2px(Vec(82, y)), module, Maestro::CH_OUTPUT_1 + i);
            if (module) outPort->voltagePtr = &module->chVoltage[i];
            addOutput(outPort);
        }
    }

    void appendContextMenu(Menu* menu) override {
        Maestro* module = dynamic_cast<Maestro*>(this->module);
        if (!module) return;

        menu->addChild(new MenuSeparator);

        // Beats per bar (submenú)
        menu->addChild(createSubmenuItem("Beats per bar",
            std::to_string(module->beatsPerBar) + " beats",
            [=](Menu* menu) {
                for (int b : {2, 3, 4, 5, 6, 7, 8}) {
                    menu->addChild(createCheckMenuItem(
                        std::to_string(b) + " beats", "",
                        [=]() { return module->beatsPerBar == b; },
                        [=]() { module->beatsPerBar = b; }
                    ));
                }
            }
        ));

        // Min active voices (submenú)
        menu->addChild(createSubmenuItem("Min active voices",
            std::to_string(module->minVoices) + " voices",
            [=](Menu* menu) {
                for (int i = 0; i <= 6; i++) {
                    menu->addChild(createCheckMenuItem(
                        std::to_string(i) + " voices", "",
                        [=]() { return module->minVoices == i; },
                        [=]() { module->minVoices = i; }
                    ));
                }
            }
        ));

        // Default input voltage (submenú)
        menu->addChild(createSubmenuItem("Default input voltage",
            module->defaultInputVoltage == 1.f ? "1V" : "10V",
            [=](Menu* menu) {
                menu->addChild(createCheckMenuItem("1V (gate/trigger)", "",
                    [=]() { return module->defaultInputVoltage == 1.f; },
                    [=]() { module->defaultInputVoltage = 1.f; }
                ));
                menu->addChild(createCheckMenuItem("10V (CV/audio)", "",
                    [=]() { return module->defaultInputVoltage == 10.f; },
                    [=]() { module->defaultInputVoltage = 10.f; }
                ));
            }
        ));

        // Active output CV mode (submenú)
        menu->addChild(createSubmenuItem("Active output CV mode",
            module->activeOutProportional ? "Proportional" : "Absolute",
            [=](Menu* menu) {
                menu->addChild(createCheckMenuItem("Proportional to active tracks", "",
                    [=]() { return module->activeOutProportional; },
                    [=]() { module->activeOutProportional = true; }
                ));
                menu->addChild(createCheckMenuItem("Absolute (1.66V per voice)", "",
                    [=]() { return !module->activeOutProportional; },
                    [=]() { module->activeOutProportional = false; }
                ));
            }
        ));

        // Skip CV mode (submenú)
        menu->addChild(createSubmenuItem("Skip CV mode",
            module->skipBinary ? "Binary" : "Probabilistic",
            [=](Menu* menu) {
                menu->addChild(createCheckMenuItem("Probabilistic", "",
                    [=]() { return !module->skipBinary; },
                    [=]() { module->skipBinary = false; }
                ));
                menu->addChild(createCheckMenuItem("Binary (on/off)", "",
                    [=]() { return module->skipBinary; },
                    [=]() { module->skipBinary = true; }
                ));
            }
        ));

        // Skip mode (submenú)
        menu->addChild(createSubmenuItem("Skip mode",
            module->skipPerChannel ? "Per channel" : "Global",
            [=](Menu* menu) {
                menu->addChild(createCheckMenuItem("Global", "",
                    [=]() { return !module->skipPerChannel; },
                    [=]() { module->skipPerChannel = false; }
                ));
                menu->addChild(createCheckMenuItem("Per channel", "",
                    [=]() { return module->skipPerChannel; },
                    [=]() { module->skipPerChannel = true; }
                ));
            }
        ));

        // Trigger duration (submenú)
        menu->addChild(createSubmenuItem("Trigger duration",
            std::to_string((int)(module->triggerDuration * 1000)) + "ms",
            [=](Menu* menu) {
                for (float ms : {1.f, 2.f, 5.f, 10.f}) {
                    menu->addChild(createCheckMenuItem(
                        std::to_string((int)ms) + "ms", "",
                        [=]() { return module->triggerDuration == ms / 1000.f; },
                        [=]() { module->triggerDuration = ms / 1000.f; }
                    ));
                }
            }
        ));

        // Reset mode (submenú)
        menu->addChild(createSubmenuItem("Reset input mode",
            module->resetForceEval ? "Force evaluate" : "Reset bars",
            [=](Menu* menu) {
                menu->addChild(createCheckMenuItem("Reset bars", "",
                    [=]() { return !module->resetForceEval; },
                    [=]() { module->resetForceEval = false; }
                ));
                menu->addChild(createCheckMenuItem("Force evaluate", "",
                    [=]() { return module->resetForceEval; },
                    [=]() { module->resetForceEval = true; }
                ));
            }
        ));
    }
};

Model* modelMaestro = createModel<Maestro, MaestroWidget>("Maestro");
