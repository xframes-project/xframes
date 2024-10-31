#include <string>
#include <nlohmann/json.hpp>

#include <xframes.h>
#include <element/element.h>

using json = nlohmann::json;

#pragma once

class XFrames;

class Widget : public Element {
    public:


        // todo: does this belong here?
        inline static OnTextChangedCallback onInputTextChange_;

        explicit Widget(XFrames* view, int id);

        const char* GetElementType() override;

        void HandleChildren(XFrames* view, const std::optional<ImRect>& viewport) override;

        void SetChildrenDisplay(XFrames* view, YGDisplay display) const;

        void PreRender(XFrames* view) override;

        void Render(XFrames* view, const std::optional<ImRect>& viewport) override;

        void PostRender(XFrames* view) override;

        void Patch(const json& elementPatchDef, XFrames* view) override;
};

