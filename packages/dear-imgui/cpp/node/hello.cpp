#include <napi.h>

#include <GLFW/glfw3.h>
#include <GLES3/gl3.h>

#include <thread>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <set>
#include <optional>
#include "imgui.h"
#include "implot.h"
#include "implot_internal.h"
#include <nlohmann/json.hpp>

#include "color_helpers.h"
#include "xframes.h"
#include "implot_renderer.h"

using json = nlohmann::json;

template <typename T>
std::vector<T> JsonToVector(std::string& data) {
    auto parsedData = json::parse(data);
    std::vector<T> vec;
    for (auto& [key, item] : parsedData.items()) {
        vec.push_back(item.template get<T>());
    }
    return vec;
}

template <typename T>
std::set<T> JsonToSet(std::string& data) {
    auto parsedData = json::parse(data);
    std::set<T> set;
    for (auto& [key, item] : parsedData.items()) {
        set.insert(item.template get<T>());
    }
    return set;
}

json IntVectorToJson(const std::vector<int>& data) {
    auto jsonArray = json::array();
    for (auto& item : data) {
        jsonArray.push_back(item);
    }
    return jsonArray;
}

json IntSetToJson(const std::set<int>& data) {
    auto jsonArray = json::array();
    for (auto& item : data) {
        jsonArray.push_back(item);
    }
    return jsonArray;
}

class Runner {
    protected:
        XFrames* m_xframes{};
        ImGuiRenderer* m_renderer{};

        std::string m_rawFontDefs;
        std::string m_assetsBasePath;
        std::optional<std::string> m_rawStyleOverridesDefs;

        Napi::ThreadSafeFunction m_tsfnOnInit;
        Napi::ThreadSafeFunction m_tsfnOnTextChange;
        Napi::ThreadSafeFunction m_tsfnOnComboChange;
        Napi::ThreadSafeFunction m_tsfnOnNumericValueChange;
        Napi::ThreadSafeFunction m_tsfnOnBooleanValueChange;
        Napi::ThreadSafeFunction m_tsfnOnMultipleNumericValuesChange;
        Napi::ThreadSafeFunction m_tsfnOnClick;

        static Runner * instance;

        Runner() {}

        // Runner(Napi::Function onInit) : m_onInit(Napi::Persistent(onInit)) {}
    public:
        static Runner* getInstance() {
            if (nullptr == instance) {
                instance = new Runner();
            }
            return instance;
        };

        ~Runner() = default;

        static void OnInit() {
            auto pRunner = getInstance();
            auto callback = [](Napi::Env env, Napi::Function jsCallback) {
                // Transform native data into JS data, passing it to the provided
                // `jsCallback` -- the TSFN's JavaScript function.
                jsCallback.Call({});
            };

            napi_status status = pRunner->m_tsfnOnInit.BlockingCall(callback);

            if (status != napi_ok) {
                // Handle error
            }
        }

        static void OnTextChange(const int id, const std::string& value) {
            auto pRunner = getInstance();
            auto callback = [id, value](Napi::Env env, Napi::Function jsCallback) {
                jsCallback.Call({Napi::Number::New(env, id), Napi::String::New(env, value)});
            };

            napi_status status = pRunner->m_tsfnOnTextChange.BlockingCall(callback);

            if (status != napi_ok) {
                // Handle error
            }
        }

        static void OnComboChange(const int id, const int value) {
            auto pRunner = getInstance();
            auto callback = [id, value](Napi::Env env, Napi::Function jsCallback) {
                jsCallback.Call({Napi::Number::New(env, id), Napi::Number::New(env, value)});
            };

            napi_status status = pRunner->m_tsfnOnComboChange.BlockingCall(callback);

            if (status != napi_ok) {
                // Handle error
            }
        }

        static void OnNumericValueChange(const int id, const float value) {
            auto pRunner = getInstance();
            auto callback = [id, value](Napi::Env env, Napi::Function jsCallback) {
                jsCallback.Call({Napi::Number::New(env, id), Napi::Number::New(env, value)});
            };

            napi_status status = pRunner->m_tsfnOnNumericValueChange.BlockingCall(callback);

            if (status != napi_ok) {
                // Handle error
            }
        }

        static void OnBooleanValueChange(const int id, const bool value) {
            auto pRunner = getInstance();
            auto callback = [id, value](Napi::Env env, Napi::Function jsCallback) {
                jsCallback.Call({Napi::Number::New(env, id), Napi::Boolean::New(env, value)});
            };

            napi_status status = pRunner->m_tsfnOnBooleanValueChange.BlockingCall(callback);

            if (status != napi_ok) {
                // Handle error
            }
        }

        // todo: improve
        static void OnMultipleNumericValuesChange(const int id, const float* values, const int numValues) {
            auto pRunner = getInstance();
            napi_status status;

            if (numValues == 2) {
                auto callback = [id, values](Napi::Env env, Napi::Function jsCallback) {
                    jsCallback.Call({
                        Napi::Number::New(env, id),
                        Napi::Number::New(env, values[0]),
                        Napi::Number::New(env, values[1])
                    });
                };

                status = pRunner->m_tsfnOnMultipleNumericValuesChange.BlockingCall(callback);
            } else if (numValues == 3) {
                auto callback = [id, values](Napi::Env env, Napi::Function jsCallback) {
                    jsCallback.Call({
                        Napi::Number::New(env, id),
                        Napi::Number::New(env, values[0]),
                        Napi::Number::New(env, values[1]),
                        Napi::Number::New(env, values[2])
                    });
                };

                status = pRunner->m_tsfnOnMultipleNumericValuesChange.BlockingCall(callback);
            } else if (numValues == 4) {
                auto callback = [id, values](Napi::Env env, Napi::Function jsCallback) {
                    jsCallback.Call({
                        Napi::Number::New(env, id),
                        Napi::Number::New(env, values[0]),
                        Napi::Number::New(env, values[1]),
                        Napi::Number::New(env, values[2]),
                        Napi::Number::New(env, values[3])
                    });
                };

                status = pRunner->m_tsfnOnMultipleNumericValuesChange.BlockingCall(callback);
            }

            if (status != napi_ok) {
                // Handle error
            }
        }

        static void OnClick(int id) {
            auto pRunner = getInstance();
            auto callback = [id](Napi::Env env, Napi::Function jsCallback) {
                jsCallback.Call({Napi::Number::New(env, id)});
            };

            napi_status status = pRunner->m_tsfnOnClick.BlockingCall(callback);

            if (status != napi_ok) {
                // Handle error
            }
        }

        // @see https://github.com/nodejs/node-addon-api/blob/main/doc/threadsafe_function.md
        void SetHandlers(
            const Napi::CallbackInfo& info,
            Napi::Function onInit,
            Napi::Function onTextChanged,
            Napi::Function onComboChanged,
            Napi::Function onNumericValueChanged,
            Napi::Function onBooleanValueChanged,
            Napi::Function onMultipleNumericValuesChanged,
            Napi::Function onClick
            ) {
            Napi::Env env = info.Env();

            m_tsfnOnInit = Napi::ThreadSafeFunction::New(
                    env,
                    onInit,
                    "onInit",
                    0,
                    1);

            m_tsfnOnTextChange = Napi::ThreadSafeFunction::New(
                    env,
                    onTextChanged,
                    "onTextChanged",
                    0,
                    1);

            m_tsfnOnComboChange = Napi::ThreadSafeFunction::New(
                    env,
                    onComboChanged,
                    "onComboChanged",
                    0,
                    1);

            m_tsfnOnNumericValueChange = Napi::ThreadSafeFunction::New(
                    env,
                    onNumericValueChanged,
                    "onNumericValueChanged",
                    0,
                    1);

            m_tsfnOnBooleanValueChange = Napi::ThreadSafeFunction::New(
                    env,
                    onBooleanValueChanged,
                    "onBooleanValueChanged",
                    0,
                    1);

            m_tsfnOnMultipleNumericValuesChange = Napi::ThreadSafeFunction::New(
                    env,
                    onMultipleNumericValuesChanged,
                    "onMultipleNumericValuesChanged",
                    0,
                    1);

            m_tsfnOnClick = Napi::ThreadSafeFunction::New(
                    env,
                    onClick,
                    "onClick",
                    0,
                    1);
        }

        void SetRawFontDefs(std::string rawFontDefs) {
            m_rawFontDefs = std::move(rawFontDefs);
        }

        void SetAssetsBasePath(std::string basePath) {
            m_assetsBasePath = std::move(basePath);
        }

        void SetRawStyleOverridesDefs(const std::string& rawStyleOverridesDefs) {
            m_rawStyleOverridesDefs.emplace(rawStyleOverridesDefs);
        }

        void init() {
            m_xframes = new XFrames("XFrames", m_rawStyleOverridesDefs);
            m_renderer = new ImPlotRenderer(
                m_xframes,
                "XFrames",
                "XFrames",
                m_rawFontDefs,
                m_assetsBasePath
            );
            // todo: do we need this?
            m_renderer->SetCurrentContext();

            m_xframes->SetEventHandlers(
                OnInit,
                OnTextChange,
                OnComboChange,
                OnNumericValueChange,
                OnMultipleNumericValuesChange,
                OnBooleanValueChange,
                OnClick
            );
        }

        void run() {
            m_renderer->Init();
        }

        void exit() const {
            // emscripten_cancel_main_loop();
            // emscripten_force_exit(0);
        }

        void resizeWindow(const int width, const int height) const {
            m_renderer->SetWindowSize(width, height);
        }

        void setElement(std::string& elementJsonAsString) const {
            m_xframes->QueueCreateElement(elementJsonAsString);
        }

        void patchElement(const int id, std::string& elementJsonAsString) const {
            m_xframes->QueuePatchElement(id, elementJsonAsString);
        }

        void elementInternalOp(const int id, std::string& elementJsonAsString) const {
            m_xframes->QueueElementInternalOp(id, elementJsonAsString);
        }

        void setChildren(const int id, const std::vector<int>& childrenIds) const {
            m_xframes->QueueSetChildren(id, childrenIds);
        }

        void appendChild(const int parentId, const int childId) const {
            m_xframes->QueueAppendChild(parentId, childId);
        }

        [[nodiscard]] std::vector<int> getChildren(const int id) const {
            return m_xframes->GetChildren(id);
        }

        [[nodiscard]] std::string getAvailableFonts() const {
            return m_renderer->GetAvailableFonts().dump();
        }

        void appendTextToClippedMultiLineTextRenderer(const int id, const std::string& data) const {
            m_xframes->AppendTextToClippedMultiLineTextRenderer(id, data);
        }

        [[nodiscard]] std::string getStyle() const {
            json style;

            style["alpha"] = m_xframes->m_appStyle.Alpha;
            style["disabledAlpha"] = m_xframes->m_appStyle.DisabledAlpha;
            style["windowPadding"] = { m_xframes->m_appStyle.WindowPadding.x, m_xframes->m_appStyle.WindowPadding.y };
            style["windowRounding"] = m_xframes->m_appStyle.WindowRounding;
            style["windowBorderSize"] = m_xframes->m_appStyle.WindowBorderSize;
            style["windowMinSize"] = { m_xframes->m_appStyle.WindowMinSize.x, m_xframes->m_appStyle.WindowMinSize.y };
            style["windowTitleAlign"] = { m_xframes->m_appStyle.WindowTitleAlign.x, m_xframes->m_appStyle.WindowTitleAlign.y };
            style["windowMenuButtonPosition"] = m_xframes->m_appStyle.WindowMenuButtonPosition;
            style["childRounding"] = m_xframes->m_appStyle.ChildRounding;
            style["childBorderSize"] = m_xframes->m_appStyle.ChildBorderSize;
            style["popupRounding"] = m_xframes->m_appStyle.PopupRounding;
            style["popupBorderSize"] = m_xframes->m_appStyle.PopupBorderSize;
            style["framePadding"] = { m_xframes->m_appStyle.FramePadding.x, m_xframes->m_appStyle.FramePadding.y };
            style["frameRounding"] = m_xframes->m_appStyle.FrameRounding;
            style["frameBorderSize"] = m_xframes->m_appStyle.FrameBorderSize;
            style["itemSpacing"] = { m_xframes->m_appStyle.ItemSpacing.x, m_xframes->m_appStyle.ItemSpacing.y };
            style["itemInnerSpacing"] = { m_xframes->m_appStyle.ItemInnerSpacing.x, m_xframes->m_appStyle.ItemInnerSpacing.y };
            style["cellPadding"] = { m_xframes->m_appStyle.CellPadding.x, m_xframes->m_appStyle.CellPadding.y };
            style["touchExtraPadding"] = { m_xframes->m_appStyle.TouchExtraPadding.x, m_xframes->m_appStyle.TouchExtraPadding.y };
            style["indentSpacing"] = m_xframes->m_appStyle.IndentSpacing;
            style["columnsMinSpacing"] = m_xframes->m_appStyle.ColumnsMinSpacing;
            style["scrollbarSize"] = m_xframes->m_appStyle.ScrollbarSize;
            style["scrollbarRounding"] = m_xframes->m_appStyle.ScrollbarRounding;
            style["grabMinSize"] = m_xframes->m_appStyle.GrabMinSize;
            style["grabRounding"] = m_xframes->m_appStyle.GrabRounding;
            style["logSliderDeadzone"] = m_xframes->m_appStyle.LogSliderDeadzone;
            style["tabRounding"] = m_xframes->m_appStyle.TabRounding;
            style["tabBorderSize"] = m_xframes->m_appStyle.TabBorderSize;
            style["tabMinWidthForCloseButton"] = m_xframes->m_appStyle.TabMinWidthForCloseButton;
            style["tabBarBorderSize"] = m_xframes->m_appStyle.TabBarBorderSize;
            style["tableAngledHeadersAngle"] = m_xframes->m_appStyle.TableAngledHeadersAngle;
            style["tableAngledHeadersTextAlign"] = { m_xframes->m_appStyle.TableAngledHeadersTextAlign.x, m_xframes->m_appStyle.TableAngledHeadersTextAlign.y };
            style["colorButtonPosition"] = m_xframes->m_appStyle.ColorButtonPosition;
            style["buttonTextAlign"] = { m_xframes->m_appStyle.ButtonTextAlign.x, m_xframes->m_appStyle.ButtonTextAlign.y };
            style["selectableTextAlign"] = { m_xframes->m_appStyle.SelectableTextAlign.x, m_xframes->m_appStyle.SelectableTextAlign.y };
            style["separatorTextPadding"] = { m_xframes->m_appStyle.SeparatorTextPadding.x, m_xframes->m_appStyle.SeparatorTextPadding.y };
            style["displayWindowPadding"] = { m_xframes->m_appStyle.DisplayWindowPadding.x, m_xframes->m_appStyle.DisplayWindowPadding.y };
            style["displaySafeAreaPadding"] = { m_xframes->m_appStyle.DisplaySafeAreaPadding.x, m_xframes->m_appStyle.DisplaySafeAreaPadding.y };
            style["mouseCursorScale"] = m_xframes->m_appStyle.MouseCursorScale;
            style["antiAliasedLines"] = m_xframes->m_appStyle.AntiAliasedLines;
            style["antiAliasedLinesUseTex"] = m_xframes->m_appStyle.AntiAliasedLinesUseTex;
            style["antiAliasedFill"] = m_xframes->m_appStyle.AntiAliasedFill;
            style["curveTessellationTol"] = m_xframes->m_appStyle.CurveTessellationTol;
            style["circleTessellationMaxError"] = m_xframes->m_appStyle.CircleTessellationMaxError;

            style["hoverStationaryDelay"] = m_xframes->m_appStyle.HoverStationaryDelay;
            style["hoverDelayShort"] = m_xframes->m_appStyle.HoverDelayShort;
            style["hoverDelayNormal"] = m_xframes->m_appStyle.HoverDelayNormal;

            style["hoverFlagsForTooltipMouse"] = m_xframes->m_appStyle.HoverFlagsForTooltipMouse;
            style["hoverFlagsForTooltipNav"] = m_xframes->m_appStyle.HoverFlagsForTooltipNav;

            style["colors"] = json::array();

            for (int i = 0; i < ImGuiCol_COUNT; i++) {
                auto maybeValue = IV4toJsonHEXATuple(m_xframes->m_appStyle.Colors[i]);

                if (maybeValue.has_value()) {
                    style["colors"].push_back(maybeValue.value());
                }
            }

            return style.dump();
        }

        void patchStyle(std::string& styleDef) const {
            m_xframes->PatchStyle(json::parse(styleDef));
        }

        void setDebug(const bool debug) const {
            m_xframes->SetDebug(debug);
        }

        void showDebugWindow() const {
            m_xframes->ShowDebugWindow();
        }
};

Runner* Runner::instance = nullptr;

void resizeWindow(const int width, const int height) {
    auto pRunner = Runner::getInstance();
    pRunner->resizeWindow(width, height);
}

void setElement(const Napi::CallbackInfo& info) {
    auto pRunner = Runner::getInstance();
    Napi::Env env = info.Env();

    if (info.Length() < 1) {
        throw Napi::TypeError::New(env, "Expected one argument");
    } else if (!info[0].IsString()) {
        throw Napi::TypeError::New(env, "Expected first arg to be string");
    }

    auto elementJson = info[0].As<Napi::String>().Utf8Value();

    pRunner->setElement(elementJson);
}

void patchElement(const Napi::CallbackInfo& info) {
    auto pRunner = Runner::getInstance();
    Napi::Env env = info.Env();

    if (info.Length() < 2) {
        throw Napi::TypeError::New(env, "Expected two arguments");
    } else if (!info[0].IsNumber()) {
        throw Napi::TypeError::New(env, "Expected first arg to be number");
    } else if (!info[1].IsString()) {
        throw Napi::TypeError::New(env, "Expected first arg to be string");
    }

    auto id = info[0].As<Napi::Number>().Int32Value();
    auto elementJson = info[1].As<Napi::String>().Utf8Value();

    pRunner->patchElement(id, elementJson);
}

void elementInternalOp(const int id, std::string elementJson) {
    auto pRunner = Runner::getInstance();
    pRunner->elementInternalOp(id, elementJson);
}

void setChildren(const Napi::CallbackInfo& info) {
    auto pRunner = Runner::getInstance();
    Napi::Env env = info.Env();

    if (info.Length() < 2) {
        throw Napi::TypeError::New(env, "Expected two arguments");
    } else if (!info[0].IsNumber()) {
        throw Napi::TypeError::New(env, "Expected first arg to be number");
    } else if (!info[1].IsString()) {
        throw Napi::TypeError::New(env, "Expected first arg to be string");
    }

    auto id = info[0].As<Napi::Number>().Int32Value();
    auto childrenIds = info[1].As<Napi::String>().Utf8Value();

    // todo: use array of numbers instead of parsing JSON
    pRunner->setChildren((int)id, JsonToVector<int>(childrenIds));
}

void appendChild(const Napi::CallbackInfo& info) {
    auto pRunner = Runner::getInstance();
    Napi::Env env = info.Env();

    if (info.Length() < 2) {
        throw Napi::TypeError::New(env, "Expected two arguments");
    } else if (!info[0].IsNumber()) {
        throw Napi::TypeError::New(env, "Expected first arg to be number");
    } else if (!info[1].IsNumber()) {
        throw Napi::TypeError::New(env, "Expected first arg to be number");
    }

    auto parentId = info[0].As<Napi::Number>().Int32Value();
    auto childId = info[1].As<Napi::Number>().Int32Value();

    pRunner->appendChild(parentId, childId);
}

std::string getChildren(const int id) {
    auto pRunner = Runner::getInstance();
    return IntVectorToJson(pRunner->getChildren(id)).dump();
}

void appendTextToClippedMultiLineTextRenderer(const int id, std::string data) {
    auto pRunner = Runner::getInstance();
    pRunner->appendTextToClippedMultiLineTextRenderer(id, data);
}

std::string getStyle() {
    auto pRunner = Runner::getInstance();
    return pRunner->getStyle();
}

void patchStyle(const Napi::CallbackInfo& info) {
    auto pRunner = Runner::getInstance();

    Napi::Env env = info.Env();

    if (info.Length() < 1) {
        throw Napi::TypeError::New(env, "Expected one argument");
    } else if (!info[0].IsString()) {
        throw Napi::TypeError::New(env, "Expected first arg to be string");
    }

    auto styleDef = info[0].As<Napi::String>().Utf8Value();


    return pRunner->patchStyle(styleDef);
}

void setDebug(const bool debug) {
    auto pRunner = Runner::getInstance();

    return pRunner->setDebug(debug);
}

void showDebugWindow(const Napi::CallbackInfo& info) {
    auto pRunner = Runner::getInstance();

    pRunner->showDebugWindow();
}

int run()
{
    auto pRunner = Runner::getInstance();

    pRunner->run();

    return 0;
}

std::thread uiThread;


std::thread nativeThread;
Napi::ThreadSafeFunction tsfn;

/**
 * [0] assets base path
 * [1] raw font definitions (stringified JSON)
 * [2] raw style override definitions (stringified JSON)
 * [3] onInit function
 * [4] onTextChanged function
 * [5] onComboChanged function
 * [6] onNumericValueChanged function
 * [7] OnBooleanValueChanged function
 * [8] OnMultipleNumericValuesChanged function
 * [8] OnClick function
 */
static Napi::Value init(const Napi::CallbackInfo& info) {
    auto pRunner = Runner::getInstance();

    Napi::Env env = info.Env();

    if (info.Length() < 10) {
        throw Napi::TypeError::New(env, "Expected ten arguments");
    } else if (!info[0].IsString()) {
        throw Napi::TypeError::New(env, "Expected first arg to be string");
    } else if (!info[1].IsString()) {
        throw Napi::TypeError::New(env, "Expected second arg to be string");
    } else if (!info[2].IsString()) {
        throw Napi::TypeError::New(env, "Expected third arg to be string");
    } else if (!info[3].IsFunction()) {
        throw Napi::TypeError::New(env, "Expected fourth arg to be function");
    } else if (!info[4].IsFunction()) {
        throw Napi::TypeError::New(env, "Expected fifth arg to be function");
    } else if (!info[5].IsFunction()) {
        throw Napi::TypeError::New(env, "Expected sixth arg to be function");
    } else if (!info[6].IsFunction()) {
        throw Napi::TypeError::New(env, "Expected seventh arg to be function");
    } else if (!info[7].IsFunction()) {
        throw Napi::TypeError::New(env, "Expected eighth arg to be function");
    } else if (!info[8].IsFunction()) {
        throw Napi::TypeError::New(env, "Expected ninth arg to be function");
    } else if (!info[9].IsFunction()) {
        throw Napi::TypeError::New(env, "Expected tenth arg to be function");
    }

    pRunner->SetAssetsBasePath(info[0].As<Napi::String>().Utf8Value());
    pRunner->SetRawFontDefs(info[1].As<Napi::String>().Utf8Value());
    pRunner->SetRawStyleOverridesDefs(info[2].As<Napi::String>().Utf8Value());

    const auto onInit = info[3].As<Napi::Function>();
    const auto onTextChanged = info[4].As<Napi::Function>();
    const auto onComboChanged = info[5].As<Napi::Function>();
    const auto onNumericValueChanged = info[6].As<Napi::Function>();
    const auto onBooleanValueChanged = info[7].As<Napi::Function>();
    const auto onMultipleNumericValuesChanged = info[8].As<Napi::Function>();
    const auto onClick = info[9].As<Napi::Function>();

    pRunner->SetHandlers(
        info,
        onInit,
        onTextChanged,
        onComboChanged,
        onNumericValueChanged,
        onBooleanValueChanged,
        onMultipleNumericValuesChanged,
        onClick
    );

    pRunner->init();

    printf("Starting UI thread\n");

    uiThread = std::thread(run);
    uiThread.detach();

    return env.Null();
}

static Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports["init"] = Napi::Function::New(env, init);
    exports["setElement"] = Napi::Function::New(env, setElement);
    exports["patchElement"] = Napi::Function::New(env, patchElement);
    exports["setChildren"] = Napi::Function::New(env, setChildren);
    exports["appendChild"] = Napi::Function::New(env, appendChild);
    exports["showDebugWindow"] = Napi::Function::New(env, showDebugWindow);
    exports["patchStyle"] = Napi::Function::New(env, patchStyle);

    return exports;
}

NODE_API_MODULE(xframes, Init)
