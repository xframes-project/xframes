#include "commit.h"
#include "ada.h"
#include "imgui.h"
#include <charconv>
#include <cmath>
#include <limits>

namespace xframes {
namespace {
using json = nlohmann::json;
void Check(bool ok, const std::string& field) {
    if (!ok) throw CommitError("invalid_props", "Invalid or missing property: " + field);
}
bool Number(const json& value) {
    return value.is_number() && std::isfinite(value.get<double>()) &&
        std::abs(value.get<double>()) <= std::numeric_limits<float>::max();
}
void Int(const json& props, const char* key, int low = std::numeric_limits<int>::min(), int high = std::numeric_limits<int>::max()) {
    if (props.contains(key)) Check(props[key].is_number_integer() && props[key] >= low && props[key] <= high, key);
}
void GuardedInt(const json& props, const char* key, int low, int high = std::numeric_limits<int>::max()) {
    if (props.contains(key) && !props[key].is_null()) Int(props, key, low, high);
}
void String(const json& props, const char* key, bool required = false) {
    if (required || props.contains(key)) Check(props.contains(key) && props[key].is_string(), key);
}
void Color(const json& value) {
    if (value.is_null()) return; // extractColor retains the existing color on removal
    Check(value.is_string() || (value.is_array() && value.size() == 2 && value[0].is_string() &&
        Number(value[1]) && value[1] >= 0 && value[1] <= 1), "color (CSS string or [CSS string, alpha])");
}
int NumericKey(const std::string& key, int limit) {
    int number = -1;
    const auto parsed = std::from_chars(key.data(), key.data() + key.size(), number);
    Check(parsed.ec == std::errc{} && parsed.ptr == key.data() + key.size() && number >= 0 && number < limit, key);
    return number;
}
void Style(const json& style) {
    // null is the established React prop-removal spelling.
    if (style.is_null()) return;
    Check(style.is_object(), "style");
    for (auto key : {"border", "borderTop", "borderRight", "borderBottom", "borderLeft"}) {
        if (!style.contains(key)) continue;
        const auto& border = style[key];
        Check(border.is_object() && border.contains("thickness") && Number(border["thickness"]), key);
        if (border.contains("color")) Color(border["color"]);
    }
    for (auto key : {"rounding", "flex", "flexGrow", "flexShrink", "aspectRatio"})
        if (style.contains(key) && !style[key].is_null()) Check(Number(style[key]), key);
    for (auto key : {"width", "height", "minWidth", "minHeight", "maxWidth", "maxHeight", "flexBasis"}) {
        if (!style.contains(key) || style[key].is_null()) continue;
        Check(Number(style[key]) || style[key].is_string(), key);
        if (style[key].is_string()) {
            const auto& s = style[key].get_ref<const std::string&>();
            if (!s.empty() && s.back() == '%') {
                try { Check(std::isfinite(std::stof(s)), key); }
                catch (const std::invalid_argument&) { throw CommitError("invalid_props", key); }
                catch (const std::out_of_range&) { throw CommitError("invalid_props", key); }
            }
        }
    }
    for (auto key : {"margin", "padding", "position", "gap"}) {
        if (!style.contains(key) || style[key].is_null()) continue;
        Check(style[key].is_object(), key);
        for (auto& item : style[key]) Check(Number(item), key);
    }
    if (style.contains("backgroundColor")) Color(style["backgroundColor"]);
    if (style.contains("roundCorners") && !style["roundCorners"].is_null()) {
        Check(style["roundCorners"].is_array(), "roundCorners");
        for (auto& value : style["roundCorners"]) Check(value.is_string(), "roundCorners");
    }
    if (style.contains("font") && !style["font"].is_null()) {
        const auto& font = style["font"];
        Check(font.is_object(), "font"); String(font, "name", true);
        Check(font.contains("size"), "font.size"); Int(font, "size", 1);
    }
    if (style.contains("colors") && !style["colors"].is_null()) {
        Check(style["colors"].is_object(), "colors");
        for (auto it = style["colors"].begin(); it != style["colors"].end(); ++it) {
            NumericKey(it.key(), ImGuiCol_COUNT); Color(it.value());
        }
    }
    if (style.contains("vars") && !style["vars"].is_null()) {
        Check(style["vars"].is_object(), "vars");
        for (auto it = style["vars"].begin(); it != style["vars"].end(); ++it) {
            NumericKey(it.key(), ImGuiStyleVar_COUNT);
            if (it.value().is_null()) continue;
            Check(Number(it.value()) || (it.value().is_array() && it.value().size() == 2 &&
                Number(it.value()[0]) && Number(it.value()[1])), "vars");
        }
    }
}
}

void ValidateCommitProps(const std::string& type, const nlohmann::json& props, bool create) {
    Check(props.is_object(), "props");
    for (auto key : {"style", "hoverStyle", "activeStyle", "disabledStyle"})
        if (props.contains(key)) Style(props[key]);
    if (props.contains("root")) {
        Check(props["root"].is_boolean(), "root");
        if (!create || type != "node") throw CommitError("immutable_identity", "root is a node creation property");
    }
    // Guarded optional widget props retain their historical null/no-op behavior.
    for (auto key : {"label", "text", "hint", "xAxisLabel", "yAxisLabel", "legendLabel", "shape", "overlay",
                     "itemId", "sliderType", "tileUrlTemplate", "attribution", "cachePath"})
        if (props.contains(key) && !props[key].is_null()) String(props, key);
    for (auto key : {"cull", "trackMouseClickEvents", "defaultChecked", "multiline", "password", "readOnly",
                     "numericOnly", "filterable", "hideable", "reorderable", "closeable", "selected", "selectable", "open"})
        if (props.contains(key) && !props[key].is_null()) Check(props[key].is_boolean(), key);
    for (auto key : {"width", "height", "min", "max", "fraction"})
        if (props.contains(key) && !props[key].is_null()) Check(Number(props[key]), key);
    if (create) {
        if (type == "bullet-text" || type == "unformatted-text" || type == "disabled-text") String(props, "text", true);
        if (type == "collapsing-header" || type == "tab-item" || type == "separator-text") String(props, "label", true);
        if (type == "tree-node") String(props, "itemId", true);
        if (type == "text-wrap") Check(props.contains("width") && Number(props["width"]), "width");
        if (type == "di-image") {
            String(props, "url", true);
            // Relative/local paths are valid desktop assets. Validate using a base so
            // the same portable shape is accepted without creating a fetch resource.
            const auto base = ada::parse<ada::url>("file:///");
            Check(bool(ada::parse<ada::url>(props["url"].get<std::string>(), &*base)), "url");
            if (props.contains("width") && props.contains("height"))
                Check(Number(props["width"]) && Number(props["height"]), "image width/height");
        }
    }
    if (type == "di-window") {
        if (props.contains("title") && (create || !props["title"].is_null())) String(props, "title");
        for (auto key : {"width", "height"}) if (props.contains(key) && (create || !props[key].is_null())) Check(Number(props[key]), key);
    }
    if (type == "combo" && create && props.contains("placeholder")) String(props, "placeholder");
    if (type.starts_with("plot-")) {
        for (auto key : {"axisAutoFit", "showLegend", "normalize"}) if (props.contains(key)) Check(props[key].is_boolean(), key);
        for (auto key : {"scaleMin", "scaleMax", "angle0"}) if (props.contains(key)) Check(Number(props[key]), key);
        Int(props, "dataPointsLimit", 1); Int(props, "legendLocation", 0, 15);
        Int(props, "xAxisDecimalDigits", 0, 9); Int(props, "yAxisDecimalDigits", 0, 9);
        Int(props, "markerStyle", -1, 9); Int(props, "xAxisScale", 0, 4); Int(props, "yAxisScale", 0, 4);
        Int(props, "colormap", 0, 15); Int(props, "bins", -4);
        if (props.contains("series") && !props["series"].is_null()) {
            const auto& series = props["series"];
            Check(series.is_array(), "series");
            for (const auto& item : series) {
                Check(item.is_object(), "series item");
                if (item.contains("label")) String(item, "label");
                Int(item, "markerStyle", -1, 9);
            }
        }
    }
    for (auto key : {"color", "defaultColor", "bullColor", "bearColor"})
        if (props.contains(key)) Color(props[key]);
    if (type == "di-table") {
        if (create || (props.contains("columns") && !props["columns"].is_null())) {
            Check(props.contains("columns") && props["columns"].is_array() && !props["columns"].empty(), "columns");
            for (const auto& column : props["columns"]) {
                Check(column.is_object(), "column"); String(column, "fieldId", true); String(column, "heading", true);
            }
        }
        if (props.contains("contextMenuItems") && !props["contextMenuItems"].is_null()) {
            Check(props["contextMenuItems"].is_array(), "contextMenuItems");
            for (const auto& item : props["contextMenuItems"]) {
                Check(item.is_object(), "contextMenuItems item"); String(item, "id", true); String(item, "label", true);
            }
        }
    }
    GuardedInt(props, "clipRows", 0); GuardedInt(props, "numberOfLines", 1); GuardedInt(props, "initialSelectedIndex", -1);
    GuardedInt(props, "minZoom", 0, 22); GuardedInt(props, "maxZoom", 0, 22);
    if (type == "multi-slider") {
        GuardedInt(props, "numValues", 2, 4); GuardedInt(props, "decimalDigits", 0, 9);
        if (props.contains("defaultValues") && !props["defaultValues"].is_null()) {
            Check(props["defaultValues"].is_array(), "defaultValues");
            for (const auto& value : props["defaultValues"]) Check(Number(value), "defaultValues");
        }
    }
    if (props.contains("tileRequestHeaders") && !props["tileRequestHeaders"].is_null()) {
        Check(props["tileRequestHeaders"].is_object(), "tileRequestHeaders");
        for (const auto& value : props["tileRequestHeaders"]) Check(value.is_string(), "tileRequestHeaders value");
    }
}
}
