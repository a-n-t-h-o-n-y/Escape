#include <esc/esc.hpp>

int main()
{
    using namespace esc;
    auto profile = resolve_profile(snapshot_environment(), true).profile;
    auto success = fg(colors::green) | bold;
    auto detail = styled("42 files", dim);
    auto message = styled(sequence("Built ", detail, " successfully"), success);
    Output output;
    output.write(encode(profile, message, "\n"));
    output.flush();
}
