#include <array>
#include <charconv>
#include <esc/query.hpp>
#include "detail/protocol.hpp"
namespace esc {
namespace {
void encode_query(detail::SequenceWriter& out, Query const& query)
{
    if (query.valueless_by_exception())
        throw std::invalid_argument{"Query: valueless request"};
    std::visit(
        [&](auto const& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::same_as<T, DeviceAttributesQuery>)
                out.text("\x1b[c");
            else if constexpr (std::same_as<T, KeyboardQuery>)
                out.text("\x1b[?u");
            else {
                if (!value.mode || value.mode > 65535)
                    throw std::invalid_argument{"Query: mode must be in 1..65535"};
                out.text("\x1b[?");
                out.number(value.mode);
                out.text("$p");
            }
        },
        query);
}
}  // namespace
auto query_to(std::span<char> output, Query const& query) -> EncodeResult
{
    detail::SequenceWriter measure;
    encode_query(measure, query);
    if (measure.size() > output.size()) return {0, measure.size()};
    detail::SequenceWriter writer{output.data()};
    encode_query(writer, query);
    return {writer.size(), writer.size()};
}
auto encode_query(Query const& query) -> std::string
{
    std::array<char, 32> bytes;
    auto result = query_to(bytes, query);
    return {bytes.data(), result.written};
}
}  // namespace esc
namespace esc::detail {
namespace {
auto number(std::string_view text, unsigned& value) -> bool
{
    if (text.empty()) return false;
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size();
}
}  // namespace
auto parse_reply(std::string_view fields, char final) -> std::optional<Reply>
{
    if (!fields.starts_with('?')) return {};
    fields.remove_prefix(1);
    unsigned first = 0;
    if (final == 'u') {
        if (number(fields, first)) return KeyboardReply{first};
    }
    else if (final == 'y' && fields.ends_with('$')) {
        fields.remove_suffix(1);
        auto delimiter = fields.find(';');
        unsigned status = 0;
        if (delimiter != fields.npos && number(fields.substr(0, delimiter), first) &&
            first && first <= 65535 && number(fields.substr(delimiter + 1), status) &&
            status <= 4)
            return ModeReply{first, static_cast<ModeStatus>(status)};
    }
    else if (final == 'c') {
        DeviceAttributesReply result;
        // A fixed count bounds allocation independently of a caller's sequence limit.
        while (!fields.empty()) {
            auto delimiter = fields.find(';');
            if (result.attributes.size() == 32 ||
                !number(fields.substr(0, delimiter), first) || first > 65535)
                return {};
            result.attributes.push_back(first);
            if (delimiter == fields.npos) return result;
            fields.remove_prefix(delimiter + 1);
        }
    }
    return {};
}
}  // namespace esc::detail
