#include "plugin.hpp"
#include <random>
#include <cmath>

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
        std::string display = editing ? editBuffer + "_" : (label ? *label : "");
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
    std::string channelLabels[MAX_CH];
    int minVoices = 0;
    int beatsPerBar = 4;
    float chVoltage[MAX_CH];

    Maestro() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        configParam(NUM_CH_PARAM, 1.f, 6.f, 6.f, "Num channels");
        getParamQuantity(NUM_CH_PARAM)->snapEnabled = true;

        configParam(DENSITY_PARAM, 0.f, 6.f, 3.f, "Density", " voices");
        getParamQuantity(DENSITY_PARAM)->snapEnabled = true;

        configParam(DENSITY_ATTV_PARAM, -1.f, 1.f, 0.f, "Density CV attenuverter");
        configParam(RESTRICT_PARAM, 0.f, 1.f, 0.f, "Randomness");

        configParam(BLOCK_PARAM, 0.f, 4.f, 0.f, "Bars");
        getParamQuantity(BLOCK_PARAM)->snapEnabled = true;

        configParam(SKIP_PARAM, 0.f, 1.f, 0.f, "Skip probability");
        configParam(FADE_IN_PARAM, 0.f, 5.f, 0.05f, "Fade In", " s");
        configParam(FADE_OUT_PARAM, 0.f, 5.f, 0.1f, "Fade Out", " s");

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
            chVoltage[i] = 0.f;
            channelLabels[i] = "CH" + std::to_string(i + 1);
        }

        configInput(CLK_INPUT, "Clock");
        configInput(DENSITY_INPUT, "Density CV");
        configInput(BLOCK_INPUT, "Bars CV");
        configInput(SKIP_INPUT, "Skip CV");
        configOutput(ACTIVE_OUTPUT, "Active voices CV");

        lastClk = false;
        beatCount = 0;
        rng.seed(std::random_device{}());
    }

    int barsValue(float raw) {
        int idx = (int)rack::math::clamp(std::round(raw), 0.f, 4.f);
        return (int)std::pow(2.f, idx);
    }

    void evaluate(int numChannels, float densityTarget, float randomness) {
        int targetVoices;
        if (randomness <= 0.001f) {
            targetVoices = (int)rack::math::clamp(std::round(densityTarget), 0.f, (float)numChannels);
        } else {
            float sigma = randomness * numChannels * 0.5f;
            std::normal_distribution<float> dist(densityTarget, sigma);
            float sampled = dist(rng);
            targetVoices = (int)rack::math::clamp(std::round(sampled), 0.f, (float)numChannels);
        }

        targetVoices = std::max(targetVoices, minVoices);
        targetVoices = std::min(targetVoices, numChannels);

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
            float barsRaw = inputs[BLOCK_INPUT].isConnected() ?
                            inputs[BLOCK_INPUT].getVoltage() / 10.f * 4.f :
                            params[BLOCK_PARAM].getValue();
            int evalEvery = barsValue(barsRaw) * beatsPerBar;

            if (beatCount >= evalEvery) {
                beatCount = 0;

                float skipProb = inputs[SKIP_INPUT].isConnected() ?
                                 rack::math::clamp(inputs[SKIP_INPUT].getVoltage() / 10.f, 0.f, 1.f) :
                                 params[SKIP_PARAM].getValue();

                std::uniform_real_distribution<float> skipDist(0.f, 1.f);
                bool doSkip = (skipProb > 0.f) && (skipDist(rng) < skipProb);

                if (!doSkip) {
                    float densityKnob = params[DENSITY_PARAM].getValue();
                    float attv = params[DENSITY_ATTV_PARAM].getValue();
                    float densityCV = inputs[DENSITY_INPUT].isConnected() ?
                                      inputs[DENSITY_INPUT].getVoltage() / 10.f * 6.f * attv : 0.f;
                    float densityTarget = rack::math::clamp(
                        std::round(densityKnob + densityCV), 0.f, (float)numChannels);
                    float randomness = params[RESTRICT_PARAM].getValue();
                    evaluate(numChannels, densityTarget, randomness);
                }
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
                    fadeLevel[i] = channelOpen[i] ? 1.f : 0.f;
                    inFade[i] = false;
                }
            }

            // Suport polifònic
            int polyCh = inputs[CH_INPUT_1 + i].isConnected() ?
                         inputs[CH_INPUT_1 + i].getChannels() : 1;
            outputs[CH_OUTPUT_1 + i].setChannels(polyCh);

            float sumVoltage = 0.f;
            for (int c = 0; c < polyCh; c++) {
                float inSignal = inputs[CH_INPUT_1 + i].isConnected() ?
                                 inputs[CH_INPUT_1 + i].getVoltage(c) : 1.f;
                float outV = inSignal * fadeLevel[i];
                outputs[CH_OUTPUT_1 + i].setVoltage(outV, c);
                sumVoltage += std::abs(outV);
            }
            // Voltatge mitjà per a l'indicador visual
            chVoltage[i] = sumVoltage / polyCh;

            // LEDs: verd=obert, groc=fade, apagat=tancat
            int lightBase = i * 2;
            if (channelOpen[i] && !inFade[i]) {
                lights[lightBase].setBrightness(1.f);
                lights[lightBase + 1].setBrightness(0.f);
                activeCount++;
            } else if (inFade[i] && fadeMode) {
                lights[lightBase].setBrightness(1.f);
                lights[lightBase + 1].setBrightness(1.f);
            } else {
                lights[lightBase].setBrightness(0.f);
                lights[lightBase + 1].setBrightness(0.f);
            }
        }

        outputs[ACTIVE_OUTPUT].setVoltage((float)activeCount / MAX_CH * 10.f);
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

        // Fila 1
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10, 18)), module, Maestro::CLK_INPUT));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(22, 18)), module, Maestro::NUM_CH_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(34, 18)), module, Maestro::DENSITY_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(46, 18)), module, Maestro::DENSITY_ATTV_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(58, 18)), module, Maestro::DENSITY_INPUT));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(70, 18)), module, Maestro::RESTRICT_PARAM));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(82, 18)), module, Maestro::ACTIVE_OUTPUT));

        // Fila 2
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(22, 34)), module, Maestro::BLOCK_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(34, 34)), module, Maestro::BLOCK_INPUT));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(46, 34)), module, Maestro::SKIP_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(58, 34)), module, Maestro::SKIP_INPUT));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(70, 34)), module, Maestro::FADE_IN_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(82, 34)), module, Maestro::FADE_OUT_PARAM));

        // Canals 1-6
        for (int i = 0; i < 6; i++) {
            float y = 51.f + i * 13.f;

            if (module) {
                ChannelLabel* label = createWidget<ChannelLabel>(mm2px(Vec(4, y - 4)));
                label->box.size = mm2px(Vec(11, 8));
                label->label = &module->channelLabels[i];
                addChild(label);
            }

            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22, y)), module, Maestro::CH_INPUT_1 + i));
            addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(33, y)), module, Maestro::PROB_PARAM_1 + i));
            addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(46, y)), module, Maestro::PROB_ATTV_PARAM_1 + i));
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(58, y)), module, Maestro::PROB_INPUT_1 + i));
            addParam(createParamCentered<CKSS>(mm2px(Vec(70, y)), module, Maestro::FADE_SWITCH_1 + i));
            addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(75, y)), module, Maestro::CH_LIGHT_1G + i * 2));

            // Port de sortida amb indicador de voltatge
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
        menu->addChild(createMenuLabel("Beats per bar"));
        for (int b : {2, 3, 4, 5, 6, 7, 8}) {
            menu->addChild(createCheckMenuItem(
                std::to_string(b) + " beats",
                "",
                [=]() { return module->beatsPerBar == b; },
                [=]() { module->beatsPerBar = b; }
            ));
        }

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Min active voices"));
        for (int i = 0; i <= 6; i++) {
            menu->addChild(createCheckMenuItem(
                std::to_string(i) + " voices",
                "",
                [=]() { return module->minVoices == i; },
                [=]() { module->minVoices = i; }
            ));
        }
    }
};

Model* modelMaestro = createModel<Maestro, MaestroWidget>("Maestro");
