#include "plugin.hpp"
#include <random>
#include <cmath>
#include <string>
#include <vector>
#include <fstream>

// Forward declaration
struct SomeStructure;

// =============================================
// StyleSection — una seccio d'un estil
// =============================================
struct StyleSection {
    std::string name = "Sec";
    float density = 3.f;
    float randomness = 0.3f;
    float length = 4.f;    // bars
    float blur = 0.2f;
    float roles[6] = {0.7f, 0.6f, 0.5f, 0.5f, 0.4f, 0.3f};
};

// Noms de veu recomanats per canal (a nivell d'estil, no de seccio)
struct StyleVoiceRoles {
    std::string voices[6] = {"CH1", "CH2", "CH3", "CH4", "CH5", "CH6"};
};

// =============================================
// Style — un estil complet amb 8 seccions
// =============================================
struct Style {
    std::string name = "Custom";
    std::string category = "custom";
    std::vector<StyleSection> sections;
    StyleVoiceRoles voiceRoles;

    void loadDefaults() {
        sections.clear();
        for (int i = 0; i < 8; i++) {
            StyleSection s;
            s.name = "Sec" + std::to_string(i + 1);
            sections.push_back(s);
        }
        // Noms per defecte
        const std::string defaults[6] = {"Kick", "Bass", "Melody", "Pad", "Lead", "FX"};
        for (int i = 0; i < 6; i++) voiceRoles.voices[i] = defaults[i];
    }

    bool loadFromJson(const std::string& path) {
        FILE* f = fopen(path.c_str(), "r");
        if (!f) return false;

        json_error_t error;
        json_t* root = json_loadf(f, 0, &error);
        fclose(f);
        if (!root) return false;

        json_t* nameJ = json_object_get(root, "name");
        if (nameJ) name = json_string_value(nameJ);

        json_t* catJ = json_object_get(root, "category");
        if (catJ) category = json_string_value(catJ);

        json_t* sectionsJ = json_object_get(root, "sections");
        if (sectionsJ && json_is_array(sectionsJ)) {
            sections.clear();
            size_t idx;
            json_t* secJ;
            json_array_foreach(sectionsJ, idx, secJ) {
                StyleSection s;
                json_t* v;
                v = json_object_get(secJ, "name");       if (v) s.name = json_string_value(v);
                v = json_object_get(secJ, "density");    if (v) s.density = (float)json_number_value(v);
                v = json_object_get(secJ, "randomness"); if (v) s.randomness = (float)json_number_value(v);
                v = json_object_get(secJ, "length");     if (v) s.length = (float)json_number_value(v);
                v = json_object_get(secJ, "blur");       if (v) s.blur = (float)json_number_value(v);
                json_t* rolesJ = json_object_get(secJ, "roles");
                if (rolesJ && json_is_array(rolesJ)) {
                    for (int i = 0; i < 6 && i < (int)json_array_size(rolesJ); i++) {
                        s.roles[i] = (float)json_number_value(json_array_get(rolesJ, i));
                    }
                }
                sections.push_back(s);
            }
        }

        // Llegir voice_roles (a nivell d'estil)
        json_t* vrJ = json_object_get(root, "voice_roles");
        if (vrJ && json_is_array(vrJ)) {
            for (int i = 0; i < 6 && i < (int)json_array_size(vrJ); i++) {
                json_t* vJ = json_array_get(vrJ, i);
                if (vJ && json_is_string(vJ)) voiceRoles.voices[i] = json_string_value(vJ);
            }
        } else {
            // Valors per defecte si no hi ha al JSON
            const std::string defaults[6] = {"Kick", "Bass", "Melody", "Pad", "Lead", "FX"};
            for (int i = 0; i < 6; i++) voiceRoles.voices[i] = defaults[i];
        }

        json_decref(root);
        return true;
    }
};

// =============================================
// Etiqueta editable (com al Maestro)
// =============================================
struct SomeStructureLabel : widget::OpaqueWidget {
    std::string* label = nullptr;
    std::string defaultText = "";
    bool editing = false;
    std::string editBuffer;

    void draw(const DrawArgs& args) override {
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 2);
        nvgFillColor(args.vg, nvgRGB(20, 20, 35));
        nvgFill(args.vg);
        if (editing) {
            nvgStrokeColor(args.vg, nvgRGB(100, 100, 220));
            nvgStrokeWidth(args.vg, 1.f);
            nvgStroke(args.vg);
        }
        nvgFontSize(args.vg, 9.f);
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgFillColor(args.vg, nvgRGB(180, 220, 100));
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
        if (editBuffer.size() < 4) editBuffer += (char)e.codepoint;
        e.consume(this);
    }
};

// =============================================
// Display de text dinamic per l'estil actiu
// =============================================
struct StyleDisplay : widget::Widget {
    SomeStructure* module = nullptr;
    std::string staticText = "Style";

    void draw(const DrawArgs& args) override;
};



// =============================================
// SomeStructure Module
// =============================================
struct DurParamQuantity : ParamQuantity {
    std::string getDisplayValueString() override {
        static const int vals[5] = {0, 1, 2, 4, 8};
        int idx = (int)rack::math::clamp(std::round(getValue()), 0.f, 4.f);
        int bars = vals[idx];
        if (bars == 0) return "Skip";
        return std::to_string(bars) + (bars == 1 ? " bar" : " bars");
    }
    std::string getLabel() override {
        return "Duration in bars (0 = Skip)";
    }
};

struct SomeStructure : Module {
    enum ParamId {
        STYLE_PARAM,
        STYLE_CAT_PARAM,   // switch Organic/Electronic
        MODE_PARAM,        // switch Auto/Manual
        BLUR_PARAM,
        BLUR_ATTV_PARAM,
        // 8 slots de seccio
        DUR_PARAM_1, DUR_PARAM_2, DUR_PARAM_3, DUR_PARAM_4,
        DUR_PARAM_5, DUR_PARAM_6, DUR_PARAM_7, DUR_PARAM_8,
        // 8 botons de salt
        BTN_PARAM_1, BTN_PARAM_2, BTN_PARAM_3, BTN_PARAM_4,
        BTN_PARAM_5, BTN_PARAM_6, BTN_PARAM_7, BTN_PARAM_8,
        PARAMS_LEN
    };
    enum InputId {
        CLK_INPUT,
        RESET_INPUT,
        BLUR_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        BUS_OUTPUT,
        DENSITY_OUTPUT,
        SECTION_CV_OUTPUT,
        SECTION_TRG_OUTPUT,
        PHRASE_TRG_OUTPUT,
        EOC_OUTPUT,
        ROLE_OUTPUT_1, ROLE_OUTPUT_2, ROLE_OUTPUT_3,
        ROLE_OUTPUT_4, ROLE_OUTPUT_5, ROLE_OUTPUT_6,
        OUTPUTS_LEN
    };
    enum LightId {
        // LEDs per cada slot: verd=actiu, groc=seguent
        SLOT_LIGHT_1, SLOT_LIGHT_2, SLOT_LIGHT_3, SLOT_LIGHT_4,
        SLOT_LIGHT_5, SLOT_LIGHT_6, SLOT_LIGHT_7, SLOT_LIGHT_8,
        LIGHTS_LEN
    };

    static const int NUM_SLOTS = 8;

    // Estat intern
    int currentSlot = 0;
    int nextSlot = 1;
    int beatCount = 0;
    int barCount = 0;
    bool lastClk = false;
    bool lastReset = false;
    bool autoMode = true;
    bool waitingForManual = false;

    // Trigger outputs (pols curt)
    float sectionTrgTimer = 0.f;
    float phraseTrgTimer = 0.f;
    int phraseEveryBars = 1; // Phrase trigger every N bars (1,2,3,4,6,8)
    int globalBarCount = 0;  // Comptador global de bars per phrase trigger
    float eocTimer = 0.f;
    static constexpr float TRG_DURATION = 0.001f;

    // Estils carregats
    Style currentStyle;
    int currentStyleIdx = 0;  // 0-4 organic, 5-10 electronic
    int styleCat = 0;         // 0=organic, 1=electronic

    // Etiquetes editables dels slots
    std::string slotLabels[NUM_SLOTS];

    // Estils Custom editables per l'usuari (A, B, C)
    Style customStyles[3];
    bool customLoaded[3] = {false, false, false};

    // Noms dels estils
    static const int NUM_ORGANIC = 5;
    static const int NUM_ELECTRONIC = 6;
    static const int NUM_CUSTOM = 3;
    const std::string organicFiles[NUM_ORGANIC] = {
        "prog_rock", "jazz", "ambient", "minimal", "dub"
    };
    const std::string electronicFiles[NUM_ELECTRONIC] = {
        "techno", "dnb", "edm", "idm", "synthwave", "dubstep"
    };
    const std::string customFiles[NUM_CUSTOM] = {
        "custom_A", "custom_B", "custom_C"
    };

    // Noms per al display
    const std::string organicNames[NUM_ORGANIC] = {
        "Prog Rock", "Jazz", "Ambient", "Minimal", "Dub"
    };
    const std::string electronicNames[NUM_ELECTRONIC] = {
        "Techno", "Drum&Bass", "EDM", "IDM/Exp", "Synthwave", "Dubstep"
    };
    const std::string customNames[NUM_CUSTOM] = {
        "Custom A", "Custom B", "Custom C"
    };

    std::string currentStyleName = "Prog Rock";
    int beatsPerBar = 4;

    SomeStructure() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        configParam(STYLE_PARAM, 0.f, 4.f, 0.f, "Style");
        getParamQuantity(STYLE_PARAM)->snapEnabled = true;

        configSwitch(STYLE_CAT_PARAM, 0.f, 2.f, 1.f, "Category", {"Organic", "Electronic", "Custom"});
        configSwitch(MODE_PARAM, 0.f, 1.f, 0.f, "Mode", {"Auto", "Manual"});

        configParam(BLUR_PARAM, 0.f, 1.f, 0.f, "Blur");
        configParam(BLUR_ATTV_PARAM, -1.f, 1.f, 0.f, "Blur CV attenuverter");

        for (int i = 0; i < NUM_SLOTS; i++) {
            configParam<DurParamQuantity>(DUR_PARAM_1 + i, 0.f, 4.f, 1.f, "Duration");
            getParamQuantity(DUR_PARAM_1 + i)->snapEnabled = true;
            configParam(BTN_PARAM_1 + i, 0.f, 1.f, 0.f, "Jump to section " + std::to_string(i + 1));
            slotLabels[i] = "Sec" + std::to_string(i + 1);
        }

        configInput(CLK_INPUT, "Clock");
        configInput(RESET_INPUT, "Reset");
        configInput(BLUR_INPUT, "Blur CV");

        configOutput(BUS_OUTPUT, "BCNmodular Bus (16ch)");
        configOutput(DENSITY_OUTPUT, "Density CV");
        configOutput(SECTION_CV_OUTPUT, "Section CV");
        configOutput(SECTION_TRG_OUTPUT, "Section trigger");
        configOutput(PHRASE_TRG_OUTPUT, "Phrase trigger");
        configOutput(EOC_OUTPUT, "End of cycle");
        for (int i = 0; i < 6; i++) {
            configOutput(ROLE_OUTPUT_1 + i, "Role weight CH" + std::to_string(i + 1));
        }

        // Carrega l'estil per defecte (Electronic - Techno)
        loadStyle(1, 0);
    }

    void onReset() override {
        // Reinicialitzar valors per defecte
        styleCat = 1;
        currentStyleIdx = 0;
        currentSlot = 0;
        nextSlot = 1;
        barCount = 0;
        beatCount = 0;
        beatsPerBar = 4;
        for (int i = 0; i < NUM_SLOTS; i++) {
            slotLabels[i] = "Sec" + std::to_string(i + 1);
        }
        for (int c = 0; c < 3; c++) {
            customLoaded[c] = false;
            customStyles[c] = Style();
        }
        phraseEveryBars = 1;
        globalBarCount = 0;
        loadStyle(1, 0);
    }

    // Converteix durada 0-4 a bars: 1,2,4,8,16 (pero limitat a 8)
    int durBars(float raw) {
        int idx = (int)rack::math::clamp(std::round(raw), 0.f, 4.f);
        static const int vals[5] = {0, 1, 2, 4, 8};
        return vals[idx];
    }

    void loadStyle(int cat, int idx) {
        styleCat = cat;
        currentStyleIdx = idx;

        std::string filename;
        std::string catFolder;
        if (cat == 0) {
            idx = rack::math::clamp(idx, 0, NUM_ORGANIC - 1);
            filename = organicFiles[idx];
            currentStyleName = organicNames[idx];
            getParamQuantity(STYLE_PARAM)->maxValue = NUM_ORGANIC - 1;
            catFolder = "organic";
        } else if (cat == 1) {
            idx = rack::math::clamp(idx, 0, NUM_ELECTRONIC - 1);
            filename = electronicFiles[idx];
            currentStyleName = electronicNames[idx];
            getParamQuantity(STYLE_PARAM)->maxValue = NUM_ELECTRONIC - 1;
            catFolder = "electronic";
        } else {
            idx = rack::math::clamp(idx, 0, NUM_CUSTOM - 1);
            currentStyleName = customNames[idx];
            getParamQuantity(STYLE_PARAM)->maxValue = NUM_CUSTOM - 1;
            // Si ja tenim el custom carregat i modificat, usar-lo directament
            if (customLoaded[idx]) {
                currentStyle = customStyles[idx];
                // Actualitzar labels des del custom
                for (int i = 0; i < NUM_SLOTS && i < (int)currentStyle.sections.size(); i++) {
                    slotLabels[i] = currentStyle.sections[i].name;
                }
                return;
            }
            filename = customFiles[idx];
            catFolder = "custom";
        }

        std::string path = asset::plugin(pluginInstance,
            "res/styles/" + catFolder + "/" + filename + ".json");

        if (!currentStyle.loadFromJson(path)) {
            currentStyle.loadDefaults();
            currentStyle.name = currentStyleName;
        }

        // Actualitzar etiquetes dels slots
        for (int i = 0; i < NUM_SLOTS; i++) {
            if (i < (int)currentStyle.sections.size()) {
                slotLabels[i] = currentStyle.sections[i].name;
            }
        }
    }

    StyleSection& getCurrentSection() {
        int idx = rack::math::clamp(currentSlot, 0, NUM_SLOTS - 1);
        if (idx < (int)currentStyle.sections.size()) {
            return currentStyle.sections[idx];
        }
        static StyleSection def;
        return def;
    }

    std::string getVoiceRole(int ch) {
        if (ch >= 0 && ch < 6) return currentStyle.voiceRoles.voices[ch];
        return "---";
    }

    void advanceToNextSlot() {
        currentSlot = nextSlot;
        nextSlot = (currentSlot + 1) % NUM_SLOTS;
        barCount = 0;
        sectionTrgTimer = TRG_DURATION;
        waitingForManual = false;
        // Skip: si la nova part té duration=0, saltar-la immediatament
        float durRaw = params[DUR_PARAM_1 + currentSlot].getValue();
        if (durBars(durRaw) == 0) {
            advanceToNextSlot(); // recursiu - salta fins trobar una part amb duration > 0
            return;
        }


        // Phrase trigger cada N bars (configurable)
        globalBarCount++;
        if (globalBarCount >= phraseEveryBars) {
            globalBarCount = 0;
            phraseTrgTimer = TRG_DURATION;
        }

        // End of Cycle quan torna al slot 0
        if (currentSlot == 0) {
            eocTimer = TRG_DURATION;
        }
    }

    void process(const ProcessArgs& args) override {
        // Llegir parametres
        int cat = (int)std::round(params[STYLE_CAT_PARAM].getValue());
        int styleIdx = (int)std::round(params[STYLE_PARAM].getValue());
        autoMode = params[MODE_PARAM].getValue() < 0.5f;

        // Recarregar estil si ha canviat
        if (cat != styleCat || styleIdx != currentStyleIdx) {
            loadStyle(cat, styleIdx);
        }

        // RESET
        bool rst = inputs[RESET_INPUT].getVoltage() > 1.f;
        bool rstRise = rst && !lastReset;
        lastReset = rst;
        if (rstRise) {
            currentSlot = 0;
            nextSlot = 1;
            barCount = 0;
            beatCount = 0;
            sectionTrgTimer = TRG_DURATION;
        }

        // Botons de salt manual
        for (int i = 0; i < NUM_SLOTS; i++) {
            if (params[BTN_PARAM_1 + i].getValue() > 0.5f) {
                nextSlot = i;
                advanceToNextSlot(); // sempre, independentment del mode
            }
        }

        // CLOCK
        bool clk = inputs[CLK_INPUT].getVoltage() > 1.f;
        bool clkRise = clk && !lastClk;
        lastClk = clk;

        if (clkRise) {
            beatCount++;
            if (beatCount >= beatsPerBar) {
                beatCount = 0;
                barCount++;

                // Durada de la seccio actual
                float durRaw = params[DUR_PARAM_1 + currentSlot].getValue();
                int targetBars = durBars(durRaw);
                // Blur: afegeix incertesa a la durada
                float blur = params[BLUR_PARAM].getValue();
                float blurCV = inputs[BLUR_INPUT].isConnected() ?
                    inputs[BLUR_INPUT].getVoltage() / 10.f * params[BLUR_ATTV_PARAM].getValue() : 0.f;
                float finalBlur = rack::math::clamp(blur + blurCV, 0.f, 1.f);

                int blurRange = (int)(finalBlur * targetBars * 0.5f);
                int minBars = std::max(1, targetBars - blurRange);
                int maxBars = targetBars + blurRange;

                bool shouldAdvance = (barCount >= minBars) &&
                    (barCount >= maxBars || (barCount >= minBars &&
                    ((float)rand() / RAND_MAX) < (1.f / (maxBars - barCount + 1))));

                if (shouldAdvance) {
                    if (autoMode) {
                        advanceToNextSlot();
                    } else {
                        waitingForManual = true;
                    }
                }
            }
        }

        // Actualitzar timers de trigger
        if (sectionTrgTimer > 0.f) sectionTrgTimer -= args.sampleTime;
        if (phraseTrgTimer > 0.f) phraseTrgTimer -= args.sampleTime;
        if (eocTimer > 0.f) eocTimer -= args.sampleTime;

        // Calcular valors del bus
        StyleSection& sec = getCurrentSection();
        float density    = rack::math::clamp(sec.density / 6.f * 10.f, 0.f, 10.f);
        float randomness = rack::math::clamp(sec.randomness * 10.f, 0.f, 10.f);
        float length     = rack::math::clamp(sec.length / 16.f * 10.f, 0.f, 10.f);
        float blurOut    = rack::math::clamp(sec.blur * 10.f, 0.f, 10.f);
        float sectionCV  = rack::math::clamp((float)currentSlot / (NUM_SLOTS - 1) * 10.f, 0.f, 10.f);
        float tensionCV  = rack::math::clamp((float)barCount / 8.f * 10.f, 0.f, 10.f);

        // BUS OUTPUT (16 canals polyfonic)
        outputs[BUS_OUTPUT].setChannels(16);
        // Bloc 1: Control de Maestro (canals 1-4, index 0-3)
        outputs[BUS_OUTPUT].setVoltage(density,    0);
        outputs[BUS_OUTPUT].setVoltage(randomness, 1);
        outputs[BUS_OUTPUT].setVoltage(length,     2);
        outputs[BUS_OUTPUT].setVoltage(blurOut,    3);
        // Bloc 2: Estructura (canals 5-9, index 4-8)
        outputs[BUS_OUTPUT].setVoltage(sectionCV,  4);
        outputs[BUS_OUTPUT].setVoltage(sectionTrgTimer > 0.f ? 10.f : 0.f, 5);
        outputs[BUS_OUTPUT].setVoltage(phraseTrgTimer > 0.f ? 10.f : 0.f, 6);
        outputs[BUS_OUTPUT].setVoltage(eocTimer > 0.f ? 10.f : 0.f, 7);
        outputs[BUS_OUTPUT].setVoltage(tensionCV,  8);
        // Canal 10: Protocol version (index 9)
        outputs[BUS_OUTPUT].setVoltage(1.f, 9);
        // Bloc 3: Role weights (canals 11-16, index 10-15)
        for (int i = 0; i < 6; i++) {
            outputs[BUS_OUTPUT].setVoltage(sec.roles[i] * 10.f, 10 + i);
        }

        // Sortides individuals
        outputs[DENSITY_OUTPUT].setVoltage(density);
        outputs[SECTION_CV_OUTPUT].setVoltage(sectionCV);
        outputs[SECTION_TRG_OUTPUT].setVoltage(sectionTrgTimer > 0.f ? 10.f : 0.f);
        outputs[PHRASE_TRG_OUTPUT].setVoltage(phraseTrgTimer > 0.f ? 10.f : 0.f);
        outputs[EOC_OUTPUT].setVoltage(eocTimer > 0.f ? 10.f : 0.f);
        // Role weight outputs individuals
        for (int i = 0; i < 6; i++) {
            outputs[ROLE_OUTPUT_1 + i].setVoltage(sec.roles[i] * 10.f);
        }

        // LEDs: verd=actiu, groc parpelleja=seguent en mode manual
        for (int i = 0; i < NUM_SLOTS; i++) {
            if (i == currentSlot) {
                lights[SLOT_LIGHT_1 + i].setBrightness(1.f);
            } else if (i == nextSlot && waitingForManual) {
                // Parpelleig
                float t = (float)barCount * 0.25f;
                lights[SLOT_LIGHT_1 + i].setBrightness(std::fmod(t * 4.f, 1.f) > 0.5f ? 0.7f : 0.f);
            } else {
                lights[SLOT_LIGHT_1 + i].setBrightness(0.f);
            }
        }
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "styleCat", json_integer(styleCat));
        json_object_set_new(rootJ, "styleIdx", json_integer(currentStyleIdx));
        json_object_set_new(rootJ, "currentSlot", json_integer(currentSlot));
        json_object_set_new(rootJ, "beatsPerBar", json_integer(beatsPerBar));
        json_t* labelsJ = json_array();
        for (int i = 0; i < NUM_SLOTS; i++) {
            json_array_append_new(labelsJ, json_string(slotLabels[i].c_str()));
        }
        json_object_set_new(rootJ, "slotLabels", labelsJ);

        // Guardar els customs modificats
        json_t* customsJ = json_array();
        for (int c = 0; c < 3; c++) {
            json_t* cJ = json_object();
            json_object_set_new(cJ, "loaded", json_boolean(customLoaded[c]));
            if (customLoaded[c]) {
                json_t* sectionsJ = json_array();
                for (auto& s : customStyles[c].sections) {
                    json_t* sJ = json_object();
                    json_object_set_new(sJ, "name", json_string(s.name.c_str()));
                    json_object_set_new(sJ, "density", json_real(s.density));
                    json_object_set_new(sJ, "randomness", json_real(s.randomness));
                    json_object_set_new(sJ, "length", json_real(s.length));
                    json_object_set_new(sJ, "blur", json_real(s.blur));
                    json_t* rolesJ = json_array();
                    for (int r = 0; r < 6; r++) json_array_append_new(rolesJ, json_real(s.roles[r]));
                    json_object_set_new(sJ, "roles", rolesJ);
                    json_array_append_new(sectionsJ, sJ);
                }
                json_object_set_new(cJ, "sections", sectionsJ);
                // Guardar voice roles
                json_t* vrJ = json_array();
                for (int r = 0; r < 6; r++) json_array_append_new(vrJ, json_string(customStyles[c].voiceRoles.voices[r].c_str()));
                json_object_set_new(cJ, "voiceRoles", vrJ);
            }
            json_array_append_new(customsJ, cJ);
        }
        json_object_set_new(rootJ, "customStyles", customsJ);
        json_object_set_new(rootJ, "phraseEveryBars", json_integer(phraseEveryBars));
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        json_t* v;
        v = json_object_get(rootJ, "styleCat");  if (v) styleCat = json_integer_value(v);
        v = json_object_get(rootJ, "styleIdx");  if (v) currentStyleIdx = json_integer_value(v);
        v = json_object_get(rootJ, "currentSlot"); if (v) currentSlot = json_integer_value(v);
        v = json_object_get(rootJ, "beatsPerBar"); if (v) beatsPerBar = json_integer_value(v);
        json_t* labelsJ = json_object_get(rootJ, "slotLabels");
        if (labelsJ) {
            for (int i = 0; i < NUM_SLOTS; i++) {
                json_t* lJ = json_array_get(labelsJ, i);
                if (lJ) slotLabels[i] = json_string_value(lJ);
            }
        }
        // Restaurar els customs modificats
        json_t* customsJ = json_object_get(rootJ, "customStyles");
        if (customsJ && json_is_array(customsJ)) {
            for (int c = 0; c < 3 && c < (int)json_array_size(customsJ); c++) {
                json_t* cJ = json_array_get(customsJ, c);
                if (!cJ) continue;
                json_t* loadedJ = json_object_get(cJ, "loaded");
                customLoaded[c] = loadedJ && json_boolean_value(loadedJ);
                if (customLoaded[c]) {
                    customStyles[c].sections.clear();
                    json_t* sectionsJ = json_object_get(cJ, "sections");
                    if (sectionsJ && json_is_array(sectionsJ)) {
                        size_t sidx;
                        json_t* sJ;
                        json_array_foreach(sectionsJ, sidx, sJ) {
                            StyleSection s;
                            json_t* v;
                            v = json_object_get(sJ, "name"); if (v) s.name = json_string_value(v);
                            v = json_object_get(sJ, "density"); if (v) s.density = (float)json_number_value(v);
                            v = json_object_get(sJ, "randomness"); if (v) s.randomness = (float)json_number_value(v);
                            v = json_object_get(sJ, "length"); if (v) s.length = (float)json_number_value(v);
                            v = json_object_get(sJ, "blur"); if (v) s.blur = (float)json_number_value(v);
                            json_t* rolesJ = json_object_get(sJ, "roles");
                            if (rolesJ) for (int r = 0; r < 6; r++) s.roles[r] = (float)json_number_value(json_array_get(rolesJ, r));
                            customStyles[c].sections.push_back(s);
                        }
                    }
                // Restaurar voice roles
                json_t* vrJ = json_object_get(cJ, "voiceRoles");
                if (vrJ && json_is_array(vrJ)) {
                    for (int r = 0; r < 6 && r < (int)json_array_size(vrJ); r++) {
                        json_t* vJ = json_array_get(vrJ, r);
                        if (vJ) customStyles[c].voiceRoles.voices[r] = json_string_value(vJ);
                    }
                }
                }
            }
        }
        json_t* pebJ = json_object_get(rootJ, "phraseEveryBars");
        if (pebJ) phraseEveryBars = json_integer_value(pebJ);
        loadStyle(styleCat, currentStyleIdx);
    }
};

// StyleDisplay draw (necessita SomeStructure definit)
void StyleDisplay::draw(const DrawArgs& args) {
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 3);
    nvgFillColor(args.vg, nvgRGB(10, 20, 5));
    nvgFill(args.vg);
    nvgStrokeColor(args.vg, nvgRGB(69, 140, 19));
    nvgStrokeWidth(args.vg, 1.f);
    nvgStroke(args.vg);

    std::string text = module ? module->currentStyleName : staticText;
    nvgFontSize(args.vg, 10.f);
    nvgFontFaceId(args.vg, APP->window->uiFont->handle);
    nvgFillColor(args.vg, nvgRGB(100, 200, 100));
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(args.vg, box.size.x / 2, box.size.y / 2, text.c_str(), NULL);
}

// =============================================
// Matriu de veus recomanades (6 canals x 2 files)
// =============================================
struct VoiceRoleDisplay : widget::Widget {
    SomeStructure* module = nullptr;

    void draw(const DrawArgs& args) override {
        // Fons
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 2);
        nvgFillColor(args.vg, nvgRGB(8, 15, 3));
        nvgFill(args.vg);
        nvgStrokeColor(args.vg, nvgRGB(50, 100, 10));
        nvgStrokeWidth(args.vg, 0.5f);
        nvgStroke(args.vg);

        float cellW = box.size.x / 6.f;
        float rowH = box.size.y / 2.f;

        nvgFontFaceId(args.vg, APP->window->uiFont->handle);

        for (int i = 0; i < 6; i++) {
            float x = i * cellW + cellW / 2.f;

            // Separador vertical
            if (i > 0) {
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, i * cellW, 2);
                nvgLineTo(args.vg, i * cellW, box.size.y - 2);
                nvgStrokeColor(args.vg, nvgRGB(30, 60, 5));
                nvgStrokeWidth(args.vg, 0.5f);
                nvgStroke(args.vg);
            }

            // Fila 1: numero de canal
            nvgFontSize(args.vg, 8.f);
            nvgFillColor(args.vg, nvgRGB(90, 140, 40));
            nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            std::string chName = "CH" + std::to_string(i + 1);
            nvgText(args.vg, x, rowH * 0.5f, chName.c_str(), NULL);

            // Fila 2: nom de veu recomanada
            std::string voiceName = "---";
            if (module && i < 6) {
                voiceName = module->getVoiceRole(i);
            }
            nvgFontSize(args.vg, 7.5f);
            nvgFillColor(args.vg, nvgRGB(140, 255, 0));
            nvgText(args.vg, x, rowH * 1.5f, voiceName.c_str(), NULL);
        }

        // Separador horitzontal
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, 2, rowH);
        nvgLineTo(args.vg, box.size.x - 2, rowH);
        nvgStrokeColor(args.vg, nvgRGB(30, 60, 5));
        nvgStrokeWidth(args.vg, 0.5f);
        nvgStroke(args.vg);
    }
};

// =============================================
// SomeStructure Widget
// =============================================
struct SomeStructureWidget : ModuleWidget {
    SomeStructureWidget(SomeStructure* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/SomeStructure.svg")));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // === COLUMNA ESQUERRA (globals) ===
        // CLK i RST
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(9, 18)), module, SomeStructure::CLK_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(9, 33)), module, SomeStructure::RESET_INPUT));

        // MODE switch
        addParam(createParamCentered<CKSS>(mm2px(Vec(6, 47)), module, SomeStructure::MODE_PARAM));

        // STYLE cat switch (Organic/Electronic)
        addParam(createParamCentered<CKSSThree>(mm2px(Vec(6, 60)), module, SomeStructure::STYLE_CAT_PARAM));

        // Style display
        StyleDisplay* sd = createWidget<StyleDisplay>(mm2px(Vec(2, 69)));
        sd->box.size = mm2px(Vec(18, 6  ));
        if (module) sd->module = module;
        else sd->staticText = "Prog Rock";
        addChild(sd);

        // STYLE knob
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(8, 81)), module, SomeStructure::STYLE_PARAM));

        // BLUR
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(8, 94)), module, SomeStructure::BLUR_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(8, 106)), module, SomeStructure::BLUR_ATTV_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8, 117)), module, SomeStructure::BLUR_INPUT));

        // === MATRIU DE VEUS RECOMANADES (zona superior central) ===
        VoiceRoleDisplay* vrd = createWidget<VoiceRoleDisplay>(mm2px(Vec(18, 12)));
        vrd->box.size = mm2px(Vec(70, 12));
        if (module) vrd->module = module;
        addChild(vrd);

        // === COLUMNA CENTRAL (8 slots) ===
        // Capcalera labels
        // y inici = 32, pas = 11mm per slot
        for (int i = 0; i < 8; i++) {
            float y = 39.f + i * 11.f;

            // LED
            addChild(createLightCentered<SmallLight<GreenLight>>(
                mm2px(Vec(25, y +0)), module, SomeStructure::SLOT_LIGHT_1 + i));

            // Etiqueta editable
            SomeStructureLabel* lbl = createWidget<SomeStructureLabel>(mm2px(Vec(27, y - 3)));
            lbl->box.size = mm2px(Vec(14, 6));
            if (module) {
                lbl->label = &module->slotLabels[i];
            } else {
                lbl->defaultText = "Sec" + std::to_string(i + 1);
            }
            addChild(lbl);

            // Knob de durada (1-8 bars)
            addParam(createParamCentered<RoundSmallBlackKnob>(
                mm2px(Vec(47, y)), module, SomeStructure::DUR_PARAM_1 + i));

            // Boto de salt
            addParam(createParamCentered<TL1105>(
                mm2px(Vec(57, y)), module, SomeStructure::BTN_PARAM_1 + i));

        }

        // === COLUMNA DRETA (sortides, fons fosc) ===
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(69, 42)), module, SomeStructure::BUS_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(69, 57)), module, SomeStructure::DENSITY_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(69, 72)), module, SomeStructure::SECTION_CV_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(69, 87)), module, SomeStructure::SECTION_TRG_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(69, 102)), module, SomeStructure::PHRASE_TRG_OUTPUT));
        // espai
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(69, 117)), module, SomeStructure::EOC_OUTPUT));
        // Role weight outputs (columna dreta, x=82)
        float roleY[6] = {42.f, 57.f, 72.f, 87.f, 102.f, 117.f};
        for (int i = 0; i < 6; i++) {
            addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(82, roleY[i])), module, SomeStructure::ROLE_OUTPUT_1 + i));
        }
    }

    void appendContextMenu(Menu* menu) override {
        SomeStructure* module = dynamic_cast<SomeStructure*>(this->module);
        if (!module) return;

        menu->addChild(new MenuSeparator);
        // Phrase trigger every N bars
        menu->addChild(createSubmenuItem("Phrase trigger every",
            std::to_string(module->phraseEveryBars) + (module->phraseEveryBars == 1 ? " bar" : " bars"),
            [=](Menu* menu) {
                for (int b : {1, 2, 3, 4, 6, 8}) {
                    menu->addChild(createCheckMenuItem(
                        std::to_string(b) + (b == 1 ? " bar" : " bars"), "",
                        [=]() { return module->phraseEveryBars == b; },
                        [=]() { module->phraseEveryBars = b; }
                    ));
                }
            }
        ));

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

        // Opcions Custom - mostrar només si categoria Custom activa
        if (module->styleCat == 2) {
            menu->addChild(new MenuSeparator);
            menu->addChild(createMenuLabel("Custom style sections:"));
            for (int slot = 0; slot < 8; slot++) {
                std::string slotName = "Part " + std::to_string(slot + 1) + ": " + module->slotLabels[slot];
                menu->addChild(createSubmenuItem(slotName, "",
                    [=](Menu* menu) {

                        // Helper per guardar canvis al customStyle
                        auto saveCustom = [=]() {
                            int cidx = module->currentStyleIdx;
                            if (cidx >= 0 && cidx < 3) {
                                module->customStyles[cidx] = module->currentStyle;
                                module->customLoaded[cidx] = true;
                            }
                        };

                        // Nom de la part
                        menu->addChild(createSubmenuItem("Name", module->slotLabels[slot],
                            [=](Menu* menu) {
                                const std::vector<std::string> types = {
                                    "Intro", "Vers", "PreCh", "Choru",
                                    "Brdge", "Break", "BrkDn", "Outro", "Custom"
                                };
                                for (const auto& t : types) {
                                    menu->addChild(createMenuItem(t, "",
                                        [=]() {
                                            module->slotLabels[slot] = t;
                                            if (slot < (int)module->currentStyle.sections.size())
                                                module->currentStyle.sections[slot].name = t;
                                            saveCustom();
                                        }
                                    ));
                                }
                            }
                        ));

                        // Density
                        int curDens = slot < (int)module->currentStyle.sections.size() ?
                            (int)module->currentStyle.sections[slot].density : 3;
                        menu->addChild(createSubmenuItem("Density", std::to_string(curDens),
                            [=](Menu* menu) {
                                for (int d = 1; d <= 6; d++) {
                                    menu->addChild(createCheckMenuItem(std::to_string(d), "",
                                        [=]() {
                                            return slot < (int)module->currentStyle.sections.size() &&
                                                (int)module->currentStyle.sections[slot].density == d;
                                        },
                                        [=]() {
                                            if (slot < (int)module->currentStyle.sections.size())
                                                module->currentStyle.sections[slot].density = (float)d;
                                            saveCustom();
                                        }
                                    ));
                                }
                            }
                        ));

                        // Voice weights per canal
                        menu->addChild(createSubmenuItem("Voice weights", "",
                            [=](Menu* menu) {
                                const std::vector<float> weightVals = {0.0f, 0.1f, 0.4f, 0.7f, 1.0f};
                                const std::vector<std::string> weightLabels = {"0.0", "0.1", "0.4", "0.7", "1.0"};
                                for (int ch = 0; ch < 6; ch++) {
                                    float curW = (slot < (int)module->currentStyle.sections.size()) ?
                                        module->currentStyle.sections[slot].roles[ch] : 0.5f;
                                    std::string curWStr = weightLabels[0];
                                    for (int wi = 0; wi < (int)weightVals.size(); wi++) {
                                        if (std::abs(curW - weightVals[wi]) < 0.05f) curWStr = weightLabels[wi];
                                    }
                                    // Nom de veu recomanat per aquest canal
                                    std::string voiceName = module->currentStyle.voiceRoles.voices[ch];
                                    menu->addChild(createSubmenuItem("CH" + std::to_string(ch+1) + " (" + voiceName + ")", curWStr,
                                        [=](Menu* menu) {
                                            for (int wi = 0; wi < (int)weightVals.size(); wi++) {
                                                float wv = weightVals[wi];
                                                menu->addChild(createCheckMenuItem(weightLabels[wi], "",
                                                    [=]() {
                                                        return slot < (int)module->currentStyle.sections.size() &&
                                                            std::abs(module->currentStyle.sections[slot].roles[ch] - wv) < 0.05f;
                                                    },
                                                    [=]() {
                                                        if (slot < (int)module->currentStyle.sections.size())
                                                            module->currentStyle.sections[slot].roles[ch] = wv;
                                                        saveCustom();
                                                    }
                                                ));
                                            }
                                        }
                                    ));
                                }
                            }
                        ));

                        // Nom de veu recomanada per canal (a nivell d'estil, no de part)
                        menu->addChild(createSubmenuItem("Recommended voice names", "",
                            [=](Menu* menu) {
                                const std::vector<std::string> voiceNames = {
                                    "Kick", "Bass", "Synth", "Pad", "Lead", "FX",
                                    "Drums", "Keys", "Guitar", "Strings", "Horns", "Vocal"
                                };
                                for (int ch = 0; ch < 6; ch++) {
                                    std::string curVoice = module->currentStyle.voiceRoles.voices[ch];
                                    menu->addChild(createSubmenuItem("CH" + std::to_string(ch+1), curVoice,
                                        [=](Menu* menu) {
                                            for (const auto& vn : voiceNames) {
                                                menu->addChild(createCheckMenuItem(vn, "",
                                                    [=]() { return module->currentStyle.voiceRoles.voices[ch] == vn; },
                                                    [=]() {
                                                        module->currentStyle.voiceRoles.voices[ch] = vn;
                                                        int cidx = module->currentStyleIdx;
                                                        if (cidx >= 0 && cidx < 3) {
                                                            module->customStyles[cidx] = module->currentStyle;
                                                            module->customLoaded[cidx] = true;
                                                        }
                                                    }
                                                ));
                                            }
                                        }
                                    ));
                                }
                            }
                        ));

                    }
                ));
            }
        }
    }
};

Model* modelSomeStructure = createModel<SomeStructure, SomeStructureWidget>("SomeStructure");
