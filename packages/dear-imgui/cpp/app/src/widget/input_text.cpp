#include <imgui.h>
#include <string>
#include <nlohmann/json.hpp>

#include "widget/input_text.h"
#include "xframes.h"

void InputText::Render(XFrames* view, const std::optional<ImRect>& viewport) {
    ImGui::PushID(m_id);
    if (m_multiline) {
        ImVec2 size(YGNodeLayoutGetWidth(m_layoutNode->m_node), YGNodeLayoutGetHeight(m_layoutNode->m_node));
        ImGui::InputTextMultiline("", &m_value, size, m_inputTextFlags, InputTextCb, (void*)this);
    } else {
        ImGui::InputTextWithHint("", m_hint.c_str(), &m_value, m_inputTextFlags, InputTextCb, (void*)this);
    }
    ImGui::PopID();
};

void InputText::Patch(const json& widgetPatchDef, XFrames* view) {
    StyledWidget::Patch(widgetPatchDef, view);

    if (widgetPatchDef.contains("hint") && widgetPatchDef["hint"].is_string()) {
        m_hint = widgetPatchDef["hint"].template get<std::string>();
    }

    bool flagsChanged = false;

    if (widgetPatchDef.contains("multiline") && widgetPatchDef["multiline"].is_boolean()) {
        m_multiline = widgetPatchDef["multiline"].template get<bool>();
    }
    if (widgetPatchDef.contains("password") && widgetPatchDef["password"].is_boolean()) {
        m_password = widgetPatchDef["password"].template get<bool>();
        flagsChanged = true;
    }
    if (widgetPatchDef.contains("readOnly") && widgetPatchDef["readOnly"].is_boolean()) {
        m_readOnly = widgetPatchDef["readOnly"].template get<bool>();
        flagsChanged = true;
    }
    if (widgetPatchDef.contains("numericOnly") && widgetPatchDef["numericOnly"].is_boolean()) {
        m_numericOnly = widgetPatchDef["numericOnly"].template get<bool>();
        flagsChanged = true;
    }

    if (flagsChanged) {
        ComputeFlags();
    }
};

bool InputText::HasInternalOps() {
    return true;
}

void InputText::HandleInternalOp(const json& opDef) {
    if (opDef.contains("op") && opDef["op"].is_string()) {
        auto op = opDef["op"].template get<std::string>();

        if (op == "setValue") {
            auto value = opDef["value"].template get<std::string>();
            SetValue(value);
        }
    }
};

int InputText::InputTextCb(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit) {
        auto pInputText = reinterpret_cast<InputText*>(data->UserData);

        std::string value = data->Buf;
        onInputTextChange_(pInputText->m_id, value);
    }

    return 0;
};
