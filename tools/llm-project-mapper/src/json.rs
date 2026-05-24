//! Tiny zero-dependency JSON value type with a recursive-descent parser and a
//! pretty serializer that mirrors `JSON.stringify(value, null, 2)`.
//!
//! Objects preserve insertion order (backed by a `Vec`) so generated maps are
//! deterministic and diff-friendly.

/// A JSON value. Numbers are split into `Int`/`Float` so integer counts print
/// without a trailing decimal.
#[derive(Debug, Clone, PartialEq)]
pub enum Json {
    Null,
    Bool(bool),
    Int(i64),
    Float(f64),
    Str(String),
    Arr(Vec<Json>),
    Obj(Vec<(String, Json)>),
}

impl Json {
    pub fn str(s: impl Into<String>) -> Json {
        Json::Str(s.into())
    }

    /// Build an array from anything iterable of `Json`.
    pub fn arr(items: impl IntoIterator<Item = Json>) -> Json {
        Json::Arr(items.into_iter().collect())
    }

    pub fn as_str(&self) -> Option<&str> {
        match self {
            Json::Str(s) => Some(s),
            _ => None,
        }
    }

    pub fn as_object(&self) -> Option<&[(String, Json)]> {
        match self {
            Json::Obj(pairs) => Some(pairs),
            _ => None,
        }
    }

    /// Look up a key in an object value.
    pub fn get(&self, key: &str) -> Option<&Json> {
        match self {
            Json::Obj(pairs) => pairs.iter().find(|(k, _)| k == key).map(|(_, v)| v),
            _ => None,
        }
    }

    /// Keys of an object value, in order. Empty for non-objects.
    pub fn keys(&self) -> Vec<String> {
        match self {
            Json::Obj(pairs) => pairs.iter().map(|(k, _)| k.clone()).collect(),
            _ => Vec::new(),
        }
    }

    /// Serialize with two-space indentation, matching `JSON.stringify(_, null, 2)`.
    pub fn to_pretty(&self) -> String {
        let mut out = String::new();
        self.write_pretty(&mut out, 0);
        out
    }

    fn write_pretty(&self, out: &mut String, indent: usize) {
        match self {
            Json::Null => out.push_str("null"),
            Json::Bool(b) => out.push_str(if *b { "true" } else { "false" }),
            Json::Int(n) => out.push_str(&n.to_string()),
            Json::Float(f) => out.push_str(&f.to_string()),
            Json::Str(s) => write_escaped(out, s),
            Json::Arr(items) => {
                if items.is_empty() {
                    out.push_str("[]");
                    return;
                }
                out.push_str("[\n");
                let pad = " ".repeat(indent + 2);
                for (i, item) in items.iter().enumerate() {
                    out.push_str(&pad);
                    item.write_pretty(out, indent + 2);
                    if i + 1 < items.len() {
                        out.push(',');
                    }
                    out.push('\n');
                }
                out.push_str(&" ".repeat(indent));
                out.push(']');
            }
            Json::Obj(pairs) => {
                if pairs.is_empty() {
                    out.push_str("{}");
                    return;
                }
                out.push_str("{\n");
                let pad = " ".repeat(indent + 2);
                for (i, (k, v)) in pairs.iter().enumerate() {
                    out.push_str(&pad);
                    write_escaped(out, k);
                    out.push_str(": ");
                    v.write_pretty(out, indent + 2);
                    if i + 1 < pairs.len() {
                        out.push(',');
                    }
                    out.push('\n');
                }
                out.push_str(&" ".repeat(indent));
                out.push('}');
            }
        }
    }
}

fn write_escaped(out: &mut String, s: &str) {
    out.push('"');
    for c in s.chars() {
        match c {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            '\u{0008}' => out.push_str("\\b"),
            '\u{000C}' => out.push_str("\\f"),
            c if (c as u32) < 0x20 => out.push_str(&format!("\\u{:04x}", c as u32)),
            c => out.push(c),
        }
    }
    out.push('"');
}

/// Parse a JSON document. Tolerant enough for `package.json`/`composer.json`.
pub fn parse(input: &str) -> Result<Json, String> {
    let chars: Vec<char> = input.chars().collect();
    let mut p = Parser { chars, pos: 0 };
    p.skip_ws();
    let value = p.parse_value()?;
    p.skip_ws();
    if p.pos != p.chars.len() {
        return Err(format!("trailing data at position {}", p.pos));
    }
    Ok(value)
}

struct Parser {
    chars: Vec<char>,
    pos: usize,
}

impl Parser {
    fn peek(&self) -> Option<char> {
        self.chars.get(self.pos).copied()
    }

    fn skip_ws(&mut self) {
        while let Some(c) = self.peek() {
            if c == ' ' || c == '\n' || c == '\t' || c == '\r' {
                self.pos += 1;
            } else {
                break;
            }
        }
    }

    fn parse_value(&mut self) -> Result<Json, String> {
        self.skip_ws();
        match self.peek() {
            Some('{') => self.parse_object(),
            Some('[') => self.parse_array(),
            Some('"') => Ok(Json::Str(self.parse_string()?)),
            Some('t') | Some('f') => self.parse_bool(),
            Some('n') => self.parse_null(),
            Some(c) if c == '-' || c.is_ascii_digit() => self.parse_number(),
            other => Err(format!("unexpected token: {:?}", other)),
        }
    }

    fn parse_object(&mut self) -> Result<Json, String> {
        self.pos += 1; // consume '{'
        let mut pairs = Vec::new();
        self.skip_ws();
        if self.peek() == Some('}') {
            self.pos += 1;
            return Ok(Json::Obj(pairs));
        }
        loop {
            self.skip_ws();
            let key = self.parse_string()?;
            self.skip_ws();
            if self.peek() != Some(':') {
                return Err("expected ':' in object".into());
            }
            self.pos += 1;
            let value = self.parse_value()?;
            pairs.push((key, value));
            self.skip_ws();
            match self.peek() {
                Some(',') => {
                    self.pos += 1;
                }
                Some('}') => {
                    self.pos += 1;
                    break;
                }
                other => return Err(format!("expected ',' or '}}', got {:?}", other)),
            }
        }
        Ok(Json::Obj(pairs))
    }

    fn parse_array(&mut self) -> Result<Json, String> {
        self.pos += 1; // consume '['
        let mut items = Vec::new();
        self.skip_ws();
        if self.peek() == Some(']') {
            self.pos += 1;
            return Ok(Json::Arr(items));
        }
        loop {
            let value = self.parse_value()?;
            items.push(value);
            self.skip_ws();
            match self.peek() {
                Some(',') => {
                    self.pos += 1;
                }
                Some(']') => {
                    self.pos += 1;
                    break;
                }
                other => return Err(format!("expected ',' or ']', got {:?}", other)),
            }
        }
        Ok(Json::Arr(items))
    }

    fn parse_string(&mut self) -> Result<String, String> {
        if self.peek() != Some('"') {
            return Err("expected string".into());
        }
        self.pos += 1;
        let mut s = String::new();
        while let Some(c) = self.peek() {
            self.pos += 1;
            match c {
                '"' => return Ok(s),
                '\\' => {
                    let esc = self.peek().ok_or("unterminated escape")?;
                    self.pos += 1;
                    match esc {
                        '"' => s.push('"'),
                        '\\' => s.push('\\'),
                        '/' => s.push('/'),
                        'n' => s.push('\n'),
                        't' => s.push('\t'),
                        'r' => s.push('\r'),
                        'b' => s.push('\u{0008}'),
                        'f' => s.push('\u{000C}'),
                        'u' => {
                            let mut code = 0u32;
                            for _ in 0..4 {
                                let h = self.peek().ok_or("bad \\u escape")?;
                                self.pos += 1;
                                code = code * 16 + h.to_digit(16).ok_or("bad hex")?;
                            }
                            s.push(char::from_u32(code).unwrap_or('\u{FFFD}'));
                        }
                        other => return Err(format!("bad escape: {}", other)),
                    }
                }
                c => s.push(c),
            }
        }
        Err("unterminated string".into())
    }

    fn parse_bool(&mut self) -> Result<Json, String> {
        if self.starts_with("true") {
            self.pos += 4;
            Ok(Json::Bool(true))
        } else if self.starts_with("false") {
            self.pos += 5;
            Ok(Json::Bool(false))
        } else {
            Err("invalid literal".into())
        }
    }

    fn parse_null(&mut self) -> Result<Json, String> {
        if self.starts_with("null") {
            self.pos += 4;
            Ok(Json::Null)
        } else {
            Err("invalid literal".into())
        }
    }

    fn parse_number(&mut self) -> Result<Json, String> {
        let start = self.pos;
        let mut is_float = false;
        if self.peek() == Some('-') {
            self.pos += 1;
        }
        while let Some(c) = self.peek() {
            match c {
                '0'..='9' => self.pos += 1,
                '.' | 'e' | 'E' | '+' | '-' => {
                    is_float = true;
                    self.pos += 1;
                }
                _ => break,
            }
        }
        let text: String = self.chars[start..self.pos].iter().collect();
        if is_float {
            text.parse::<f64>()
                .map(Json::Float)
                .map_err(|e| e.to_string())
        } else {
            text.parse::<i64>()
                .map(Json::Int)
                .map_err(|e| e.to_string())
        }
    }

    fn starts_with(&self, lit: &str) -> bool {
        lit.chars()
            .enumerate()
            .all(|(i, c)| self.chars.get(self.pos + i).copied() == Some(c))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_and_reads_objects() {
        let v = parse(r#"{"name":"demo","deps":{"a":1,"b":2},"flag":true}"#).unwrap();
        assert_eq!(v.get("name").and_then(Json::as_str), Some("demo"));
        assert_eq!(v.get("deps").unwrap().keys(), vec!["a", "b"]);
    }

    #[test]
    fn pretty_prints_with_two_spaces() {
        let v = Json::Obj(vec![
            ("a".into(), Json::Int(1)),
            ("b".into(), Json::arr([Json::Int(1), Json::Int(2)])),
        ]);
        let expected = "{\n  \"a\": 1,\n  \"b\": [\n    1,\n    2\n  ]\n}";
        assert_eq!(v.to_pretty(), expected);
    }

    #[test]
    fn escapes_strings() {
        let v = Json::str("a\"b\\c\n");
        assert_eq!(v.to_pretty(), "\"a\\\"b\\\\c\\n\"");
    }

    #[test]
    fn empty_containers() {
        assert_eq!(Json::Arr(vec![]).to_pretty(), "[]");
        assert_eq!(Json::Obj(vec![]).to_pretty(), "{}");
    }
}
