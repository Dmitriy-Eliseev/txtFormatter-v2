# txtFormatter

**txtFormatter** is a text formatting utility that uses an HTML-like markup language. Input files use the `.txtm` extension, and the program produces formatted `.txt` output.

## Quick Start

```bash
make                    # Build
./txtfmt                # Process all .txtm files in the current directory
```

The program reads all `.txtm` files in the current directory and creates formatted `.txt` files.

## Markup Language

### Headers

| Tag | Description |
|-----|-------------|
| `<h1>Text</h1>` | Level 1 heading (centered, `=` separators) |
| `<h2>Text</h2>` | Level 2 heading (centered, `=` separators) |
| `<h3>Text</h3>` | Level 3 heading (centered, `-` separators) |
| `<h4>Text</h4>` | Level 4 heading (text + `-` underline) |

Attributes: `<h1 *>Heading</h1>` — use `*` as the separator character.

### Text Alignment

| Tag | Description |
|-----|-------------|
| `<center>Text</center>` | Centered text |
| `<right>Text</right>` | Right-aligned text |

### Paragraphs and Frames

| Tag | Description |
|-----|-------------|
| `<p>Text</p>` | Paragraph with indentation |
| `<p R>Text</p>` | Right-aligned paragraph |
| `<frame>Text</frame>` | Text in a decorative border box |

### Lists

| Tag | Description |
|-----|-------------|
| `<list>Items</list>` | Numbered list |
| `<list *>Items</list>` | Bulleted list with asterisks |
| `<list ->Items</list>` | Bulleted list with dashes |
| `<list #>Items</list>` | Bulleted list with hash marks |

Each list item is on a separate line.

### Tables

```
<table>
Header1|Header2|Header3
Data1|Data2|Data3
</table>
```

Attributes:
- `nb` — no border
- `nc` — no calculations
- `na` — no number alignment (no right-alignment for numbers)

Table cells can contain mathematical expressions: `2+2`, `sqrt(144)`, `sin(pi/2)`.

### Histograms

```
<histogram>
Gold|2505.25
Silver|27.835
</histogram>
```

Attribute: `<histogram *>` — use `*` as the bar character.

### Calculations

```
<calc>
1+2
10*10
sqrt(144)
</calc>
```

Attribute `s` — show expression and result: `<calc s>1+2</calc>` → `1+2 = 3`

Supported functions: `sin`, `cos`, `tan`, `sqrt`, `log`, `exp`, `pow`, `fac`, `pi`, `e`.

### Separators and Blank Lines

| Tag | Description |
|-----|-------------|
| `<sep>` | Separator line (`-`) |
| `<sep +>` | Separator made of `+` characters |
| `<lines 5>` | Insert 5 blank lines |

### Date and Time

| Tag | Description |
|-----|-------------|
| `<date>` | Current date (DD.MM.YYYY) |
| `<time>` | Current time (HH:MM:SS) |
| `<datetime>` | Date and time |

### Document Width

| Tag | Description |
|-----|-------------|
| `<doc_width 100>` | Set document width to 100 characters |
| `<default_width>` | Reset to default width (80) |

### File Insertion

```
<insert path/to/file.txt>
```

Inserts the contents of a text file. Paths containing `..` are rejected for security. Path traversal outside the current working directory is also blocked.

### Tag Nesting

Tags can be nested:

```
<center><frame>Centered frame</frame></center>
<frame><calc>100+200</calc></frame>
```

## Example

```
<h1>My Document</h1>

<p>This is a paragraph that will be formatted.</p>

<frame>
Important text in a frame.
<center>Centered line</center>
</frame>

<table>
Item|Price|Quantity
Apples|25.5|10
</table>
```

## Limitations

- Maximum file size: 100 MB
- Document width: 10–250 characters
- Nested tags inside tables and histograms are not supported

## Running Tests

```bash
cd tests
bash run_tests.sh       # Integration tests (30 tests)
bash stress_test.sh     # Stress tests (47 tests)
bash fuzz_test.sh       # Fuzz testing (150 tests)
```

## License

GPL-3.0. Copyright (C) 2024 Dmitriy Eliseev.
