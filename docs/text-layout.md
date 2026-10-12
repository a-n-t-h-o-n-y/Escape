# Graphemes and terminal cell widths

Escape provides allocation-free extended grapheme segmentation and deterministic
cell-width estimates for UTF-8 and UTF-32. Unicode properties are pinned to
17.0.0, exposed as `layout_unicode_version`. There is no locale, environment,
terminal access, normalization, or external runtime dependency in these APIs.
Include `<esc/layout.hpp>` or `<esc/esc.hpp>`.

## Borrowed grapheme views

```cpp
std::string_view text = "e\xcc\x81\xe4\xb8\xad"; // e + acute, then 中
for (auto cluster : esc::graphemes(text)) {
    // cluster borrows text; its size is bytes, not scalars or cells.
}
auto first = esc::next_grapheme(text); // the complete e + acute cluster
```

`graphemes` returns a forward, borrowed range of string views. UTF-32 overloads
return UTF-32 views, with lengths and error offsets in scalars. Input storage
must outlive all views and iterators and must not change during iteration.
Calling with a temporary owning string leaves dangling views after that string
is destroyed, just like constructing a string_view from it.
`next_grapheme` returns the first cluster (empty for empty input). Both follow the
Unicode 17 default extended grapheme rules GB1..GB999, including Hangul, Indic
conjuncts (GB9c), emoji ZWJ context, and paired regional indicators. A glyph or
cluster does not promise one cell; this does not change GlyphString's scalar model.

Segmentation is incremental over complete encoded text, not a streaming decoder:
partial UTF-8 sequences are invalid, and appending future combining marks can
change the final boundary. Each selected cluster and the scalar needed to decide
its boundary are validated. An invalid suffix is reported when iteration reaches
it, rather than by a preliminary scan of the whole range. UnicodeError offsets
are relative to the input supplied to the range or next_grapheme. No replacement
characters or byte fragments are substituted.

Styled text also has zero-copy `next_grapheme`, `cell_width`, and `grapheme_width`
overloads for `span<Glyph const>` (including implicit spans from GlyphString).
Offsets and lengths count glyphs. `next_grapheme` returns the original glyph span,
preserving brushes even when styles change within a cluster. Only symbols are
validated/measured; brushes are ignored. Iterate with `subspan(cluster.size())`
to advance without extracting or allocating a separate plain string.

## Cell-width policy

```cpp
esc::WidthPolicy policy;
policy.ambiguous = esc::AmbiguousWidth::Two;
auto cells = esc::cell_width(text, policy);
if (cells) {
    // Width estimate for plain text under this policy.
}
```

`cell_width` returns `optional<size_t>` for any length of plain text.
`grapheme_width` uses the same policy for exactly one cluster: empty input is zero;
multiple clusters throw invalid_argument. Both validate the entire input, even
when an earlier control has already made the result unknown. Invalid encoding
throws UnicodeError with a byte/scalar offset; invalid policies throw (invalid
scalar overrides throw UnicodeError). Overflow throws length_error.

Defaults and alternatives:

| Setting | Default | Alternative |
| --- | --- | --- |
| East Asian ambiguous | One cell | Two cells |
| Emoji presentation | Two cells | One cell |
| Joined cluster | Maximum constituent width | Sum of constituent widths |
| Orphan combining marks | Zero | One per all-zero mark cluster |
| Unassigned scalars | One (unless marked wide by Unicode) | Unknown |

Wide/fullwidth scalars use two cells; other visible scalars use one unless a
policy or override changes that. Unicode marks (Mn/Mc/Me), format characters,
and default-ignorable scalars normally contribute zero. Emoji_Presentation uses
the emoji policy, including modifiers. Emoji scalars immediately followed by VS16
also use it; VS15 suppresses the default emoji presentation but does not change
an independent East Asian wide property. A literal keycap sequence [#*0-9],
optional VS16, U+20E3 uses emoji width in Maximum mode. Flags, modifier sequences,
and emoji ZWJ/tag sequences share their cluster's maximum constituent width.
This is a declared rendering model, not RGI sequence recognition or a claim
that every joined sequence is supported by the terminal/font.

Additive mode sums scalar contributions, so a family sequence can occupy more
than two cells and emoji modifiers can contribute their own cells. It is useful
for hosts that do not combine those forms, but remains an estimate of that host.
For example, a Devanagari conjunct is estimated at one in Maximum mode and two
in Additive mode; Hangul jamo can likewise differ. An orphan-mark policy only
changes an otherwise zero-width cluster containing Extend/SpacingMark; a lone
ZWJ remains zero. A custom positive mark width participates in ordinary width
calculation instead.

C0/C1 controls (including NUL, tab, newline, DEL, and Escape), and Unicode
line/paragraph separators return nullopt. Scalar overrides cannot give these a
fixed width. ANSI sequences are not stripped; pass styled text's plain content.
Tabs, wrapping, cursor motion, font shaping, and line layout need application
context and are not simulated. Format characters are modeled as non-advancing;
applications are responsible for bidi/control presentation decisions.

## Explicit terminal and font overrides

```cpp
constexpr esc::ScalarWidthOverride overrides[] = {
    {0xE0B0, 1}, // an application-selected private-use font symbol
};
esc::WidthPolicy policy;
policy.overrides = overrides;
auto cells = esc::cell_width("\xee\x82\xb0", policy);
```

Overrides borrow sorted, unique entries with valid scalars and widths 0..2.
They take precedence over ordinary properties, unassigned handling, and emoji
presentation. They participate in cluster Maximum/Additive calculation; they do
not alter grapheme boundaries or make cursor-dependent controls measurable.
The table is validated once per measurement, then searched by binary search.
Keep the storage alive throughout the call. Private-use scalars are assigned;
without an override they follow their East Asian ambiguous property.

A terminal's Unicode version, font, ambiguous-width configuration, shaping, and
emoji support can differ from this model. No profile or probe currently discovers
those choices. Select policies from verified application knowledge. Applications
needing sequence-specific tailoring can iterate `graphemes`, supply their own
width for selected clusters, and use `grapheme_width` for the rest.

## Data, performance, and verification

The algorithm uses constant-space forward context for Indic linking, emoji ZWJ,
and regional indicators, avoiding backward rescans on long combining sequences.
Width calculation segments and measures in one pass. Printable ASCII runs use
a direct path while preserving the final scalar for Unicode lookahead. ASCII properties bypass
lookup; non-ASCII uses one binary search of 2,138 coalesced property intervals
per decoded scalar. Measurement and iteration allocate nothing, including first
use; no lazy initialization or mutable global cache is involved. Range iteration
also decodes the boundary lookahead on the next step. Policy table validation is
O(overrides); scalar searches are O(log overrides).

`tools/unicode/generate.py` regenerates the checked-in tables and conformance
fixture from official files supplied by the developer. `sources.json` records
pinned URLs and SHA-256 checksums; generation is offline and standard-library-only.
Normal builds do not download data or run Python. Hangul syllable break properties
are computed arithmetically. Unicode-derived files carry `LICENSE.unicode`, also
installed in the package's licenses directory.

Tests execute all 766 official Unicode 17 GraphemeBreakTest cases through both
UTF-8 and UTF-32 APIs. Additional independent goldens cover width policies,
controls, malformed input/error offsets, font overrides, emoji presentation,
keycaps, Hangul/Indic, range borrowing, and 100,000 combining marks under an
allocation-rejecting scope. UTF-8/UTF-32 width results are compared over every
conformance fixture. These establish the declared policy, not emulator rendering.

The protocol basis is [Unicode 17 UAX #29](https://www.unicode.org/reports/tr29/tr29-47.html),
[East Asian Width](https://www.unicode.org/reports/tr11/tr11-44.html), and
[Unicode emoji properties](https://www.unicode.org/Public/17.0.0/ucd/emoji/emoji-data.txt).
East Asian Width explicitly requires tailoring for terminal use.

Local verification on macOS uses Apple Clang 15 release, independent public
headers, installed static/shared/subproject consumers, and ASan/UBSan layout
tests. Linux and Windows cross-builds compile and link the portable tests; native
runtime tests and font/emulator width comparisons are still separate validation.
