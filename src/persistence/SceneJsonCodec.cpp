#include "persistence/SceneJsonCodec.h"

#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include <windows.h>

namespace ws {
namespace {

std::string ToUtf8(std::wstring_view value) {
    if (value.empty()) return {};
    const int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) throw std::runtime_error("Invalid UTF-16 text");
    std::string result(static_cast<std::size_t>(required), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
            result.data(), required, nullptr, nullptr) != required) {
        throw std::runtime_error("UTF-8 conversion failed");
    }
    return result;
}

std::wstring FromUtf8(std::string_view value) {
    if (value.empty()) return {};
    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0) throw std::runtime_error("Invalid UTF-8 text");
    std::wstring result(static_cast<std::size_t>(required), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
            result.data(), required) != required) {
        throw std::runtime_error("UTF-16 conversion failed");
    }
    return result;
}

void WriteEscapedString(std::ostream& output, std::string_view value) {
    output << '"';
    for (const unsigned char character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (character < 0x20) {
                output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<unsigned int>(character) << std::dec << std::setfill(' ');
            } else {
                output << static_cast<char>(character);
            }
            break;
        }
    }
    output << '"';
}

class JsonReader {
public:
    explicit JsonReader(std::string_view input) : input_(input) {}

    void Expect(char expected) {
        SkipWhitespace();
        if (position_ >= input_.size() || input_[position_] != expected) Fail("Unexpected JSON token");
        ++position_;
    }

    bool Consume(char expected) {
        SkipWhitespace();
        if (position_ < input_.size() && input_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    [[nodiscard]] std::string ReadString() {
        SkipWhitespace();
        if (position_ >= input_.size() || input_[position_] != '"') Fail("Expected JSON string");
        ++position_;
        std::string result;
        while (position_ < input_.size()) {
            const unsigned char character = static_cast<unsigned char>(input_[position_++]);
            if (character == '"') return result;
            if (character < 0x20) Fail("Control character in JSON string");
            if (character != '\\') {
                result.push_back(static_cast<char>(character));
                continue;
            }
            if (position_ >= input_.size()) Fail("Incomplete JSON escape");
            const char escaped = input_[position_++];
            switch (escaped) {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case 'u': AppendUnicodeEscape(result); break;
            default: Fail("Invalid JSON escape");
            }
        }
        Fail("Unterminated JSON string");
    }

    [[nodiscard]] double ReadNumber() {
        SkipWhitespace();
        const std::size_t start = position_;
        while (position_ < input_.size()) {
            const char character = input_[position_];
            if ((character >= '0' && character <= '9') || character == '-' || character == '+' ||
                character == '.' || character == 'e' || character == 'E') {
                ++position_;
            } else {
                break;
            }
        }
        if (start == position_) Fail("Expected JSON number");
        double value{};
        const char* begin = input_.data() + start;
        const char* end = input_.data() + position_;
        const auto converted = std::from_chars(begin, end, value, std::chars_format::general);
        if (converted.ec != std::errc{} || converted.ptr != end || !std::isfinite(value)) {
            Fail("Invalid JSON number");
        }
        return value;
    }

    [[nodiscard]] int ReadInteger() {
        const double value = ReadNumber();
        if (std::floor(value) != value || value < static_cast<double>(std::numeric_limits<int>::min()) ||
            value > static_cast<double>(std::numeric_limits<int>::max())) Fail("Expected integer");
        return static_cast<int>(value);
    }

    [[nodiscard]] bool ReadBoolean() {
        SkipWhitespace();
        if (input_.substr(position_, 4) == "true") { position_ += 4; return true; }
        if (input_.substr(position_, 5) == "false") { position_ += 5; return false; }
        Fail("Expected JSON boolean");
    }

    void SkipValue() {
        SkipWhitespace();
        if (position_ >= input_.size()) Fail("Expected JSON value");
        if (input_[position_] == '"') { static_cast<void>(ReadString()); return; }
        if (input_[position_] == '{') {
            Expect('{');
            if (Consume('}')) return;
            do { static_cast<void>(ReadString()); Expect(':'); SkipValue(); } while (Consume(','));
            Expect('}');
            return;
        }
        if (input_[position_] == '[') {
            Expect('[');
            if (Consume(']')) return;
            do { SkipValue(); } while (Consume(','));
            Expect(']');
            return;
        }
        if (input_.substr(position_, 4) == "true" || input_.substr(position_, 4) == "null") {
            position_ += 4;
            return;
        }
        if (input_.substr(position_, 5) == "false") { position_ += 5; return; }
        static_cast<void>(ReadNumber());
    }

    [[nodiscard]] bool AtEnd() {
        SkipWhitespace();
        return position_ == input_.size();
    }

private:
    [[noreturn]] void Fail(const char* message) const { throw std::runtime_error(message); }

    void SkipWhitespace() {
        while (position_ < input_.size()) {
            const char character = input_[position_];
            if (character != ' ' && character != '\t' && character != '\r' && character != '\n') break;
            ++position_;
        }
    }

    [[nodiscard]] unsigned int ReadHexQuad() {
        if (position_ + 4 > input_.size()) Fail("Incomplete Unicode escape");
        unsigned int value{};
        for (int index = 0; index < 4; ++index) {
            const char character = input_[position_++];
            value <<= 4;
            if (character >= '0' && character <= '9') value += static_cast<unsigned int>(character - '0');
            else if (character >= 'a' && character <= 'f') value += static_cast<unsigned int>(character - 'a' + 10);
            else if (character >= 'A' && character <= 'F') value += static_cast<unsigned int>(character - 'A' + 10);
            else Fail("Invalid Unicode escape");
        }
        return value;
    }

    void AppendUnicodeEscape(std::string& output) {
        unsigned int codePoint = ReadHexQuad();
        if (codePoint >= 0xD800 && codePoint <= 0xDBFF) {
            if (position_ + 2 > input_.size() || input_[position_] != '\\' || input_[position_ + 1] != 'u') {
                Fail("Missing low surrogate");
            }
            position_ += 2;
            const unsigned int low = ReadHexQuad();
            if (low < 0xDC00 || low > 0xDFFF) Fail("Invalid low surrogate");
            codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
        } else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF) {
            Fail("Unexpected low surrogate");
        }

        if (codePoint <= 0x7F) output.push_back(static_cast<char>(codePoint));
        else if (codePoint <= 0x7FF) {
            output.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
            output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else if (codePoint <= 0xFFFF) {
            output.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
            output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else {
            output.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
            output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
    }

    std::string_view input_;
    std::size_t position_{};
};

template <typename Handler>
void ReadObject(JsonReader& reader, Handler&& handler) {
    reader.Expect('{');
    if (reader.Consume('}')) return;
    do {
        const std::string key = reader.ReadString();
        reader.Expect(':');
        handler(key);
    } while (reader.Consume(','));
    reader.Expect('}');
}

GridPlacement ReadGrid(JsonReader& reader) {
    GridPlacement grid{};
    ReadObject(reader, [&](const std::string& key) {
        if (key == "column") grid.column = reader.ReadInteger();
        else if (key == "row") grid.row = reader.ReadInteger();
        else if (key == "columnSpan") grid.columnSpan = reader.ReadInteger();
        else if (key == "rowSpan") grid.rowSpan = reader.ReadInteger();
        else reader.SkipValue();
    });
    if (grid.column < 0 || grid.row < 0 || grid.columnSpan < 1 || grid.rowSpan < 1) {
        throw std::runtime_error("Invalid grid placement");
    }
    return grid;
}

FreePlacement ReadFree(JsonReader& reader) {
    FreePlacement placement{};
    ReadObject(reader, [&](const std::string& key) {
        if (key == "x") placement.x = static_cast<float>(reader.ReadNumber());
        else if (key == "y") placement.y = static_cast<float>(reader.ReadNumber());
        else if (key == "width") placement.width = static_cast<float>(reader.ReadNumber());
        else if (key == "height") placement.height = static_cast<float>(reader.ReadNumber());
        else reader.SkipValue();
    });
    if (!std::isfinite(placement.x) || !std::isfinite(placement.y) ||
        !std::isfinite(placement.width) || !std::isfinite(placement.height) ||
        placement.width <= 0.0f || placement.height <= 0.0f) {
        throw std::runtime_error("Invalid free placement");
    }
    return placement;
}

WidgetAppearance ReadAppearance(JsonReader& reader) {
    WidgetAppearance appearance{};
    bool surfaceRead = false;
    bool tintRead = false;
    bool legacyGlass = appearance.glassEnabled;
    ReadObject(reader, [&](const std::string& key) {
        if (key == "mode") {
            const std::string mode = reader.ReadString();
            if (mode == "dark") appearance.mode = AppearanceMode::Dark;
            else if (mode == "light") appearance.mode = AppearanceMode::Light;
            else throw std::runtime_error("Unknown appearance mode");
        } else if (key == "surface") {
            const std::string surface = reader.ReadString();
            if (surface == "frosted") appearance.surface = SurfaceMode::Frosted;
            else if (surface == "transparent") appearance.surface = SurfaceMode::Transparent;
            else if (surface == "solid") appearance.surface = SurfaceMode::Solid;
            else throw std::runtime_error("Unknown surface mode");
            surfaceRead = true;
        } else if (key == "glass") legacyGlass = reader.ReadBoolean();
        else if (key == "opacity") appearance.opacity = static_cast<float>(reader.ReadNumber());
        else if (key == "blurRadius") appearance.blurRadius = static_cast<float>(reader.ReadNumber());
        else if (key == "cornerRadius") appearance.cornerRadius = static_cast<float>(reader.ReadNumber());
        else if (key == "innerPadding") appearance.innerPadding = static_cast<float>(reader.ReadNumber());
        else if (key == "border") appearance.borderEnabled = reader.ReadBoolean();
        else if (key == "shadow") appearance.shadowEnabled = reader.ReadBoolean();
        else if (key == "fontFamily") appearance.fontFamily = FromUtf8(reader.ReadString());
        else if (key == "tintColor") {
            const int tint = reader.ReadInteger();
            if (tint < 0 || tint > 0x00FFFFFF) throw std::runtime_error("Invalid tint color");
            appearance.tintColor = static_cast<std::uint32_t>(tint);
            tintRead = true;
        }
        else reader.SkipValue();
    });
    if (!surfaceRead) appearance.surface = legacyGlass ? SurfaceMode::Frosted : SurfaceMode::Solid;
    if (!tintRead && appearance.mode == AppearanceMode::Light) appearance.tintColor = 0xF0F2F5;
    appearance.glassEnabled = appearance.surface == SurfaceMode::Frosted;
    if (appearance.opacity < 0.0f || appearance.opacity > 1.0f || appearance.blurRadius < 0.0f ||
        appearance.cornerRadius < 0.0f || appearance.innerPadding < 0.0f ||
        !std::isfinite(appearance.opacity) ||
        !std::isfinite(appearance.blurRadius) || !std::isfinite(appearance.cornerRadius) ||
        !std::isfinite(appearance.innerPadding) || appearance.fontFamily.empty() ||
        appearance.fontFamily.size() > 128) {
        throw std::runtime_error("Invalid appearance values");
    }
    return appearance;
}

WidgetState ReadState(JsonReader& reader) {
    WidgetState state;
    ReadObject(reader, [&](const std::string& key) {
        state.emplace(FromUtf8(key), FromUtf8(reader.ReadString()));
    });
    return state;
}

WidgetPersistenceRecord ReadWidget(JsonReader& reader) {
    WidgetPersistenceRecord record{};
    ReadObject(reader, [&](const std::string& key) {
        if (key == "instanceId") record.instanceId = reader.ReadString();
        else if (key == "typeId") record.typeId = reader.ReadString();
        else if (key == "monitorId") record.monitorId = FromUtf8(reader.ReadString());
        else if (key == "layoutMode") {
            const std::string mode = reader.ReadString();
            if (mode == "grid") record.layoutMode = LayoutMode::Grid;
            else if (mode == "free") record.layoutMode = LayoutMode::Free;
            else throw std::runtime_error("Unknown layout mode");
        } else if (key == "grid") record.grid = ReadGrid(reader);
        else if (key == "free") record.free = ReadFree(reader);
        else if (key == "locked") record.locked = reader.ReadBoolean();
        else if (key == "contentScale") record.contentScale = static_cast<float>(reader.ReadNumber());
        else if (key == "appearance") record.appearance = ReadAppearance(reader);
        else if (key == "useGeneralAppearance") record.useGeneralAppearance = reader.ReadBoolean();
        else if (key == "state") record.widgetState = ReadState(reader);
        else reader.SkipValue();
    });
    if (record.instanceId.empty() || record.typeId.empty() ||
        !std::isfinite(record.contentScale) || record.contentScale <= 0.0f) {
        throw std::runtime_error("Widget record is missing required values");
    }
    return record;
}

std::wstring ErrorToWide(const std::exception& error) {
    const std::string message = error.what();
    return std::wstring(message.begin(), message.end());
}

bool IsValidAppearance(const WidgetAppearance& appearance) {
    return std::isfinite(appearance.opacity) && appearance.opacity >= 0.0f &&
        appearance.opacity <= 1.0f && std::isfinite(appearance.blurRadius) &&
        appearance.blurRadius >= 0.0f && std::isfinite(appearance.cornerRadius) &&
        appearance.cornerRadius >= 0.0f && std::isfinite(appearance.innerPadding) &&
        appearance.innerPadding >= 0.0f && !appearance.fontFamily.empty() &&
        appearance.fontFamily.size() <= 128 && appearance.tintColor <= 0x00FFFFFFu;
}

bool SameAppearance(const WidgetAppearance& left, const WidgetAppearance& right) {
    return left.mode == right.mode && left.surface == right.surface &&
        left.glassEnabled == right.glassEnabled && left.opacity == right.opacity &&
        left.blurRadius == right.blurRadius && left.cornerRadius == right.cornerRadius &&
        left.innerPadding == right.innerPadding && left.borderEnabled == right.borderEnabled &&
        left.shadowEnabled == right.shadowEnabled && left.fontFamily == right.fontFamily &&
        left.tintColor == right.tintColor;
}

void ValidateForEncoding(const WidgetPersistenceRecord& record) {
    static_cast<void>(FromUtf8(record.instanceId));
    static_cast<void>(FromUtf8(record.typeId));
    if (record.instanceId.empty() || record.typeId.empty() || record.monitorId.empty() ||
        record.grid.column < 0 || record.grid.row < 0 || record.grid.columnSpan < 1 || record.grid.rowSpan < 1 ||
        !std::isfinite(record.free.x) || !std::isfinite(record.free.y) ||
        !std::isfinite(record.free.width) || !std::isfinite(record.free.height) ||
        record.free.width <= 0.0f || record.free.height <= 0.0f ||
        !std::isfinite(record.contentScale) || record.contentScale <= 0.0f ||
        !IsValidAppearance(record.appearance)) {
        throw std::runtime_error("Scene contains invalid widget values");
    }
}

void WriteAppearance(std::ostringstream& output, const WidgetAppearance& appearance) {
    const SurfaceMode surface = appearance.surface == SurfaceMode::Frosted &&
        !appearance.glassEnabled ? SurfaceMode::Solid : appearance.surface;
    output << "{\"mode\": \""
           << (appearance.mode == AppearanceMode::Dark ? "dark" : "light")
           << "\", \"surface\": \""
           << (surface == SurfaceMode::Frosted ? "frosted" :
               surface == SurfaceMode::Transparent ? "transparent" : "solid")
           << "\", \"glass\": " << (surface == SurfaceMode::Frosted ? "true" : "false")
           << ", \"opacity\": " << appearance.opacity
           << ", \"blurRadius\": " << appearance.blurRadius
           << ", \"cornerRadius\": " << appearance.cornerRadius
           << ", \"innerPadding\": " << appearance.innerPadding
           << ", \"border\": " << (appearance.borderEnabled ? "true" : "false")
           << ", \"shadow\": " << (appearance.shadowEnabled ? "true" : "false")
           << ", \"fontFamily\": ";
    WriteEscapedString(output, ToUtf8(appearance.fontFamily));
    output << ", \"tintColor\": " << appearance.tintColor << '}';
}

} // namespace

std::string SceneJsonCodec::Encode(const WidgetSceneSnapshot& snapshot) {
    if (!IsValidAppearance(snapshot.generalAppearance)) {
        throw std::runtime_error("Scene contains invalid general appearance values");
    }
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<float>::max_digits10);
    output << "{\n  \"schemaVersion\": " << kCurrentSchemaVersion
           << ",\n  \"generalAppearance\": ";
    WriteAppearance(output, snapshot.generalAppearance);
    output << ",\n  \"widgets\": [";
    for (std::size_t index = 0; index < snapshot.size(); ++index) {
        const auto& widget = snapshot[index];
        ValidateForEncoding(widget);
        output << (index == 0 ? "\n" : ",\n") << "    {\n      \"instanceId\": ";
        WriteEscapedString(output, widget.instanceId);
        output << ",\n      \"typeId\": "; WriteEscapedString(output, widget.typeId);
        output << ",\n      \"monitorId\": "; WriteEscapedString(output, ToUtf8(widget.monitorId));
        output << ",\n      \"layoutMode\": \""
               << (widget.layoutMode == LayoutMode::Grid ? "grid" : "free") << "\",";
        output << "\n      \"grid\": {\"column\": " << widget.grid.column
               << ", \"row\": " << widget.grid.row
               << ", \"columnSpan\": " << widget.grid.columnSpan
               << ", \"rowSpan\": " << widget.grid.rowSpan << "},";
        output << "\n      \"free\": {\"x\": " << widget.free.x << ", \"y\": " << widget.free.y
               << ", \"width\": " << widget.free.width << ", \"height\": " << widget.free.height << "},";
        output << "\n      \"locked\": " << (widget.locked ? "true" : "false")
               << ",\n      \"contentScale\": " << widget.contentScale
               << ",\n      \"useGeneralAppearance\": "
               << (widget.useGeneralAppearance ? "true" : "false")
               << ",\n      \"appearance\": ";
        WriteAppearance(output, widget.appearance);
        output << ',';
        output << "\n      \"state\": {";
        std::size_t stateIndex{};
        for (const auto& [key, value] : widget.widgetState) {
            if (stateIndex++ != 0) output << ", ";
            WriteEscapedString(output, ToUtf8(key));
            output << ": ";
            WriteEscapedString(output, ToUtf8(value));
        }
        output << "}\n    }";
    }
    if (!snapshot.empty()) output << '\n';
    output << "  ]\n}\n";
    return output.str();
}

std::optional<DecodedScene> SceneJsonCodec::Decode(
    std::string_view json, std::wstring& errorMessage) noexcept {
    try {
        JsonReader reader(json);
        DecodedScene scene{};
        bool hasVersion = false;
        bool hasWidgets = false;
        ReadObject(reader, [&](const std::string& key) {
            if (key == "schemaVersion") {
                scene.schemaVersion = reader.ReadInteger();
                hasVersion = true;
            } else if (key == "generalAppearance") {
                scene.generalAppearance = ReadAppearance(reader);
            } else if (key == "widgets") {
                reader.Expect('[');
                if (!reader.Consume(']')) {
                    do { scene.widgets.push_back(ReadWidget(reader)); } while (reader.Consume(','));
                    reader.Expect(']');
                }
                hasWidgets = true;
            } else {
                reader.SkipValue();
            }
        });
        if (!reader.AtEnd()) throw std::runtime_error("Trailing JSON content");
        if (!hasVersion || !hasWidgets) throw std::runtime_error("Missing scene schema fields");
        if (scene.schemaVersion < 0 || scene.schemaVersion > kCurrentSchemaVersion) {
            throw std::runtime_error("Unsupported scene schema version");
        }
        if (scene.schemaVersion < 2 && !scene.widgets.empty()) {
            // Preserve the old scene exactly while treating its first common
            // appearance as the new general default. Matching widgets join the
            // general group; genuinely customized widgets remain opted out.
            scene.generalAppearance = scene.widgets.front().appearance;
        }
        for (auto& widget : scene.widgets) {
            static_cast<void>(FromUtf8(widget.instanceId));
            static_cast<void>(FromUtf8(widget.typeId));
            if (scene.schemaVersion == 0 && widget.monitorId.empty()) widget.monitorId = L"primary";
            if (scene.schemaVersion < 2) {
                widget.useGeneralAppearance = SameAppearance(
                    widget.appearance, scene.generalAppearance);
            }
            if (widget.monitorId.empty()) throw std::runtime_error("Widget record is missing required values");
        }
        scene.widgets.generalAppearance = scene.generalAppearance;
        scene.schemaVersion = kCurrentSchemaVersion;
        std::set<std::string> instanceIds;
        for (const auto& widget : scene.widgets) {
            if (!instanceIds.insert(widget.instanceId).second) throw std::runtime_error("Duplicate widget instance ID");
        }
        errorMessage.clear();
        return scene;
    } catch (const std::exception& error) {
        errorMessage = ErrorToWide(error);
        return std::nullopt;
    }
}

} // namespace ws
