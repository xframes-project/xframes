#include <string>
#include <nlohmann/json.hpp>

#include <xframes.h>
#include <element/element.h>

using json = nlohmann::json;

#pragma once

class ReactImgui;

class Widget : public Element {
    public:


        // todo: does this belong here?
        inline static OnTextChangedCallback onInputTextChange_;

        explicit Widget(ReactImgui* view, int id);

        const char* GetElementType() override;

        void HandleChildren(ReactImgui* view, const std::optional<ImRect>& viewport) override;

        void SetChildrenDisplay(ReactImgui* view, YGDisplay display) const;

        void PreRender(ReactImgui* view) override;

        void Render(ReactImgui* view, const std::optional<ImRect>& viewport) override;

        void PostRender(ReactImgui* view) override;

        void Patch(const json& elementPatchDef, ReactImgui* view) override;
};

