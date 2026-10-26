// Copyright 2026 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

//go:build ignore
// +build ignore

package main

// print-brotli-tables.go prints the std/brotli/common_tables.wuffs file. It
// extracts the tables (the static dictionary, the dictionary word transforms,
// the context lookup tables, the insert-and-copy command lookup table, etc.)
// from the reference C implementation's source code (github.com/google/brotli)
// so that none of the values are hand-typed.
//
// Usage: go run print-brotli-tables.go [brotli_c_dir] > common_tables.wuffs
//
// The optional brotli_c_dir argument defaults to /home/user/brotli-ref/c. It
// is the "c" directory of a brotli source tree (the one that contains the
// "common" and "dec" directories).

import (
	"bytes"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	dir := "/home/user/brotli-ref/c"
	if len(os.Args) > 1 {
		dir = os.Args[1]
	}

	readFile := func(name string) (string, error) {
		b, err := os.ReadFile(filepath.Join(dir, name))
		if err != nil {
			return "", err
		}
		return stripComments(string(b)), nil
	}

	// ---- common/dictionary.bin and common/dictionary.c

	dictionary, err := os.ReadFile(filepath.Join(dir, "common/dictionary.bin"))
	if err != nil {
		return err
	}
	dictionaryC, err := readFile("common/dictionary.c")
	if err != nil {
		return err
	}
	// The kBrotliDictionary initializer is preceded by an #if / #else pair
	// of declarations. Use the last one. Its first two brace-delimited
	// fields are size_bits_by_length and offsets_by_length.
	i := strings.LastIndex(dictionaryC, "kBrotliDictionary =")
	if i < 0 {
		return errors.New("could not find kBrotliDictionary")
	}
	dictionaryBody, err := findBraces(dictionaryC[i:], "kBrotliDictionary =")
	if err != nil {
		return err
	}
	sizeBits, err := parseIntsAfter(dictionaryBody, "", 32)
	if err != nil {
		return err
	}
	j := strings.IndexByte(dictionaryBody, '}')
	if j < 0 {
		return errors.New("could not find offsets_by_length")
	}
	offsets, err := parseIntsAfter(dictionaryBody[j+1:], "", 32)
	if err != nil {
		return err
	}
	if n := offsets[31]; n != int64(len(dictionary)) {
		return fmt.Errorf("dictionary.bin length (%d) does not match offsets_by_length[31] (%d)",
			len(dictionary), n)
	}
	for i := 0; i < 31; i++ {
		want := offsets[i]
		if sizeBits[i] != 0 {
			want += int64(i) << sizeBits[i]
		}
		if offsets[i+1] != want {
			return fmt.Errorf("inconsistent offsets_by_length[%d]", i+1)
		}
	}

	// ---- common/transform.h and common/transform.c

	transformH, err := readFile("common/transform.h")
	if err != nil {
		return err
	}
	enumValues, err := parseEnum(transformH, "BrotliWordTransformType")
	if err != nil {
		return err
	}
	transformC, err := readFile("common/transform.c")
	if err != nil {
		return err
	}
	prefixSuffix, err := parseCStringAfter(transformC, "kPrefixSuffix")
	if err != nil {
		return err
	}
	// The C array's declared size includes the implicit trailing NUL.
	prefixSuffix = append(prefixSuffix, 0)
	if want, err := parseDeclaredSize(transformC, "kPrefixSuffix"); err != nil {
		return err
	} else if want != len(prefixSuffix) {
		return fmt.Errorf("kPrefixSuffix: have %d bytes, want %d", len(prefixSuffix), want)
	}
	prefixSuffixMap, err := parseIntsAfter(transformC, "kPrefixSuffixMap", -1)
	if err != nil {
		return err
	}
	transformsData, err := parseValuesAfter(transformC, "kTransformsData", enumValues)
	if err != nil {
		return err
	}
	if (len(transformsData) % 3) != 0 {
		return errors.New("kTransformsData: length is not a multiple of 3")
	}
	numTransforms := len(transformsData) / 3
	transformPrefixID := make([]int64, numTransforms)
	transformType := make([]int64, numTransforms)
	transformSuffixID := make([]int64, numTransforms)
	for i := 0; i < numTransforms; i++ {
		transformPrefixID[i] = transformsData[3*i+0]
		transformType[i] = transformsData[3*i+1]
		transformSuffixID[i] = transformsData[3*i+2]
	}
	// The prefix and suffix strings are length-prefixed. Check that they're
	// all within bounds.
	maxAffixLength := 0
	for _, m := range prefixSuffixMap {
		if int(m) >= len(prefixSuffix) {
			return errors.New("kPrefixSuffixMap: out of bounds")
		}
		n := int(prefixSuffix[m])
		if (int(m) + 1 + n) > len(prefixSuffix) {
			return errors.New("kPrefixSuffix: out of bounds")
		}
		if maxAffixLength < n {
			maxAffixLength = n
		}
	}

	// ---- common/context.c

	contextC, err := readFile("common/context.c")
	if err != nil {
		return err
	}
	contextLookup, err := parseIntsAfter(contextC, "_kBrotliContextLookupTable", 2048)
	if err != nil {
		return err
	}

	// ---- common/constants.c

	constantsC, err := readFile("common/constants.c")
	if err != nil {
		return err
	}
	prefixCodeRanges, err := parseIntsAfter(constantsC, "_kBrotliPrefixCodeRanges", 2*26)
	if err != nil {
		return err
	}
	blockLengthOffsets := make([]int64, 26)
	blockLengthNBits := make([]int64, 26)
	for i := range blockLengthOffsets {
		blockLengthOffsets[i] = prefixCodeRanges[2*i+0]
		blockLengthNBits[i] = prefixCodeRanges[2*i+1]
	}

	// ---- dec/prefix_inc.h

	prefixIncH, err := readFile("dec/prefix_inc.h")
	if err != nil {
		return err
	}
	cmdLut, err := parseIntsAfter(prefixIncH, "kCmdLut", 6*704)
	if err != nil {
		return err
	}
	cmdInsertExtraBits := make([]int64, 704)
	cmdCopyExtraBits := make([]int64, 704)
	cmdImplicitZeroDistance := make([]int64, 704)
	cmdDistanceContext := make([]int64, 704)
	cmdInsertOffset := make([]int64, 704)
	cmdCopyOffset := make([]int64, 704)
	for i := 0; i < 704; i++ {
		cmdInsertExtraBits[i] = cmdLut[6*i+0]
		cmdCopyExtraBits[i] = cmdLut[6*i+1]
		switch cmdLut[6*i+2] {
		case 0:
			cmdImplicitZeroDistance[i] = 1
		case -1:
			cmdImplicitZeroDistance[i] = 0
		default:
			return errors.New("kCmdLut: unexpected distance_code")
		}
		cmdDistanceContext[i] = cmdLut[6*i+3]
		cmdInsertOffset[i] = cmdLut[6*i+4]
		cmdCopyOffset[i] = cmdLut[6*i+5]
	}

	// ---- dec/decode.c

	decodeC, err := readFile("dec/decode.c")
	if err != nil {
		return err
	}
	codeLengthCodeOrder, err := parseIntsAfter(decodeC, "kCodeLengthCodeOrder", 18)
	if err != nil {
		return err
	}
	codeLengthPrefixLength, err := parseIntsAfter(decodeC, "kCodeLengthPrefixLength", 16)
	if err != nil {
		return err
	}
	codeLengthPrefixValue, err := parseIntsAfter(decodeC, "kCodeLengthPrefixValue", 16)
	if err != nil {
		return err
	}

	// ---- dec/huffman.c

	huffmanC, err := readFile("dec/huffman.c")
	if err != nil {
		return err
	}
	reverseBits, err := parseIntsAfter(huffmanC, "kReverseBits", 256)
	if err != nil {
		return err
	}

	// ---- Print.

	w := &bytes.Buffer{}
	w.WriteString(header)
	fmt.Fprintf(w, "// The tables in this file were generated by script/print-brotli-tables.go\n")
	fmt.Fprintf(w, "// from the brotli (C) source code. Do not edit by hand.\n\n")

	fmt.Fprintf(w, "// NUM_TRANSFORMS is the number of RFC 7932 dictionary word transforms.\n")
	fmt.Fprintf(w, "pri const NUM_TRANSFORMS : base.u32 = %d\n\n", numTransforms)
	fmt.Fprintf(w, "// MAX_AFFIX_LENGTH is the maximum length of a transform's prefix or suffix.\n")
	fmt.Fprintf(w, "pri const MAX_AFFIX_LENGTH : base.u32 = %d\n\n", maxAffixLength)

	printTable(w, "From dec/decode.c kCodeLengthCodeOrder.",
		"CODE_LENGTH_CODE_ORDER", "base.u8[..= 17]", codeLengthCodeOrder, 18, false)
	printTable(w, "From dec/decode.c kCodeLengthPrefixLength.",
		"CODE_LENGTH_PREFIX_LENGTH", "base.u8[..= 4]", codeLengthPrefixLength, 16, false)
	printTable(w, "From dec/decode.c kCodeLengthPrefixValue.",
		"CODE_LENGTH_PREFIX_VALUE", "base.u8[..= 5]", codeLengthPrefixValue, 16, false)
	printTable(w, "From dec/huffman.c kReverseBits.",
		"REVERSE_BITS", "base.u8", reverseBits, 16, true)
	printTable(w, "From common/constants.c _kBrotliPrefixCodeRanges[i].offset.",
		"BLOCK_LENGTH_OFFSETS", "base.u32[..= 0xFFFF]", blockLengthOffsets, 13, false)
	printTable(w, "From common/constants.c _kBrotliPrefixCodeRanges[i].nbits.",
		"BLOCK_LENGTH_NBITS", "base.u32[..= 24]", blockLengthNBits, 13, false)

	printTable(w, "From dec/prefix_inc.h kCmdLut[i].insert_len_extra_bits.",
		"CMD_INSERT_LEN_EXTRA_BITS", "base.u8[..= 24]", cmdInsertExtraBits, 16, false)
	printTable(w, "From dec/prefix_inc.h kCmdLut[i].copy_len_extra_bits.",
		"CMD_COPY_LEN_EXTRA_BITS", "base.u8[..= 24]", cmdCopyExtraBits, 16, false)
	printTable(w, "From dec/prefix_inc.h kCmdLut[i].distance_code: 1 means 0\n"+
		"// (an implicit distance code 0, reusing the last distance) and 0 means -1\n"+
		"// (an explicit distance code follows in the bitstream).",
		"CMD_IMPLICIT_ZERO_DISTANCE", "base.u8[..= 1]", cmdImplicitZeroDistance, 16, false)
	printTable(w, "From dec/prefix_inc.h kCmdLut[i].context.",
		"CMD_DISTANCE_CONTEXT", "base.u8[..= 3]", cmdDistanceContext, 16, false)
	printTable(w, "From dec/prefix_inc.h kCmdLut[i].insert_len_offset.",
		"CMD_INSERT_LEN_OFFSET", "base.u32[..= 0xFFFF]", cmdInsertOffset, 12, true)
	printTable(w, "From dec/prefix_inc.h kCmdLut[i].copy_len_offset.",
		"CMD_COPY_LEN_OFFSET", "base.u32[..= 0xFFFF]", cmdCopyOffset, 12, true)

	printTable(w, "From common/context.c _kBrotliContextLookupTable. For context mode m\n"+
		"// (0 ..= 3), the lookups for the last and second-last bytes are at offsets\n"+
		"// ((m << 9) | 0x000) and ((m << 9) | 0x100).",
		"CONTEXT_LOOKUP", "base.u8[..= 63]", contextLookup, 16, false)

	printTable(w, "From common/transform.c kPrefixSuffix, including the implicit trailing\n"+
		"// NUL. Each prefix or suffix is a length byte followed by that many bytes.",
		"PREFIX_SUFFIX", "base.u8", toInt64s(prefixSuffix), 16, true)
	printTable(w, "From common/transform.c kPrefixSuffixMap.",
		"PREFIX_SUFFIX_MAP", fmt.Sprintf("base.u32[..= %d]", len(prefixSuffix)-1),
		prefixSuffixMap, 16, true)
	printTable(w, "From common/transform.c kTransformsData[(3 * i) + 0].",
		"TRANSFORM_PREFIX_ID", fmt.Sprintf("base.u8[..= %d]", len(prefixSuffixMap)-1),
		transformPrefixID, 16, false)
	printTable(w, "From common/transform.c kTransformsData[(3 * i) + 1]. The values are\n"+
		"// common/transform.h's BrotliWordTransformType enum values.",
		"TRANSFORM_TYPE", "base.u8[..= 22]", transformType, 16, false)
	printTable(w, "From common/transform.c kTransformsData[(3 * i) + 2].",
		"TRANSFORM_SUFFIX_ID", fmt.Sprintf("base.u8[..= %d]", len(prefixSuffixMap)-1),
		transformSuffixID, 16, false)

	printTable(w, "From common/dictionary.c kBrotliDictionary.size_bits_by_length.",
		"DICTIONARY_SIZE_BITS_BY_LENGTH", "base.u32[..= 24]", sizeBits, 16, false)
	printTable(w, "From common/dictionary.c kBrotliDictionary.offsets_by_length.",
		"DICTIONARY_OFFSETS_BY_LENGTH", fmt.Sprintf("base.u32[..= %d]", len(dictionary)),
		offsets, 8, false)
	printTable(w, "From common/dictionary.bin, the RFC 7932 static dictionary.",
		"DICTIONARY", "base.u8", toInt64s(dictionary), 16, true)

	// Drop the final blank line.
	_, err = os.Stdout.Write(bytes.TrimSuffix(w.Bytes(), []byte("\n")))
	return err
}

func toInt64s(b []byte) []int64 {
	ret := make([]int64, len(b))
	for i, x := range b {
		ret[i] = int64(x)
	}
	return ret
}

func printTable(w *bytes.Buffer, comment string, name string, elemType string,
	values []int64, perLine int, hex bool) {

	// Fit as many elements per line (up to perLine) as keeps each line at
	// most 100 columns wide, assuming that every element is as wide as the
	// largest one. Round that down to a multiple of 4, for readability.
	maxValue := int64(0)
	for _, v := range values {
		if maxValue < v {
			maxValue = v
		}
	}
	format := "%d,"
	if hex {
		if maxValue > 0xFF {
			format = "0x%04X,"
		} else {
			format = "0x%02X,"
		}
	}
	cellWidth := len(fmt.Sprintf(format, maxValue))
	if n := (100 - 8 + 1) / (cellWidth + 1); perLine > n {
		perLine = n &^ 3
	}

	fmt.Fprintf(w, "// %s\n", comment)
	fmt.Fprintf(w, "pri const %s : roarray[%d] %s = [\n", name, len(values), elemType)
	for i, v := range values {
		if (i % perLine) == 0 {
			w.WriteString("        ")
		} else {
			w.WriteString(" ")
		}
		fmt.Fprintf(w, format, v)
		if ((i % perLine) == (perLine - 1)) || (i == (len(values) - 1)) {
			w.WriteString("\n")
		}
	}
	w.WriteString("]\n\n")
}

// stripComments removes C "/* etc */" and "// etc" comments. It does not
// handle comment-like sequences inside string or character literals, other
// than skipping over double-quoted string literals.
func stripComments(s string) string {
	b := &strings.Builder{}
	for i := 0; i < len(s); {
		switch {
		case s[i] == '"':
			j := i + 1
			for ; j < len(s); j++ {
				if s[j] == '\\' {
					j++
				} else if s[j] == '"' {
					break
				}
			}
			b.WriteString(s[i : j+1])
			i = j + 1
		case strings.HasPrefix(s[i:], "/*"):
			j := strings.Index(s[i+2:], "*/")
			if j < 0 {
				return b.String()
			}
			b.WriteByte(' ')
			i += 2 + j + 2
		case strings.HasPrefix(s[i:], "//"):
			j := strings.IndexByte(s[i:], '\n')
			if j < 0 {
				return b.String()
			}
			i += j
		default:
			b.WriteByte(s[i])
			i++
		}
	}
	return b.String()
}

// findBraces returns the contents of the first "{ etc }" block (with
// balanced nested braces) after the first occurrence of name.
func findBraces(s string, name string) (string, error) {
	i := strings.Index(s, name)
	if i < 0 {
		return "", fmt.Errorf("could not find %q", name)
	}
	s = s[i+len(name):]
	i = strings.IndexByte(s, '{')
	if i < 0 {
		return "", fmt.Errorf("could not find %q's '{'", name)
	}
	depth := 0
	for j := i; j < len(s); j++ {
		switch s[j] {
		case '{':
			depth++
		case '}':
			depth--
			if depth == 0 {
				return s[i+1 : j], nil
			}
		}
	}
	return "", fmt.Errorf("could not find %q's '}'", name)
}

func parseIntsAfter(s string, name string, wantLen int) ([]int64, error) {
	return parseValuesAfterWithLen(s, name, nil, wantLen)
}

func parseValuesAfter(s string, name string, idents map[string]int64) ([]int64, error) {
	return parseValuesAfterWithLen(s, name, idents, -1)
}

func parseValuesAfterWithLen(s string, name string, idents map[string]int64, wantLen int) ([]int64, error) {
	body, err := findBraces(s, name)
	if err != nil {
		return nil, err
	}
	body = strings.NewReplacer("{", " ", "}", " ").Replace(body)
	ret := []int64(nil)
	for _, tok := range strings.Split(body, ",") {
		tok = strings.TrimSpace(tok)
		if tok == "" {
			continue
		}
		if v, ok := idents[tok]; ok {
			ret = append(ret, v)
			continue
		}
		v, err := strconv.ParseInt(tok, 0, 64)
		if err != nil {
			return nil, fmt.Errorf("%s: could not parse %q", name, tok)
		}
		ret = append(ret, v)
	}
	if (wantLen >= 0) && (len(ret) != wantLen) {
		return nil, fmt.Errorf("%s: have %d elements, want %d", name, len(ret), wantLen)
	}
	return ret, nil
}

var declaredSizeRE = regexp.MustCompile(`\[(\d+)\]`)

func parseDeclaredSize(s string, name string) (int, error) {
	i := strings.Index(s, name)
	if i < 0 {
		return 0, fmt.Errorf("could not find %q", name)
	}
	m := declaredSizeRE.FindStringSubmatch(s[i:])
	if m == nil {
		return 0, fmt.Errorf("could not find %q's declared size", name)
	}
	return strconv.Atoi(m[1])
}

func parseEnum(s string, name string) (map[string]int64, error) {
	body, err := findBraces(s, name)
	if err != nil {
		return nil, err
	}
	ret := map[string]int64{}
	next := int64(0)
	for _, tok := range strings.Split(body, ",") {
		tok = strings.TrimSpace(tok)
		if tok == "" {
			continue
		}
		key, value := tok, next
		if i := strings.IndexByte(tok, '='); i >= 0 {
			key = strings.TrimSpace(tok[:i])
			v, err := strconv.ParseInt(strings.TrimSpace(tok[i+1:]), 0, 64)
			if err != nil {
				return nil, fmt.Errorf("%s: could not parse %q", name, tok)
			}
			value = v
		}
		ret[key] = value
		next = value + 1
	}
	return ret, nil
}

// parseCStringAfter parses the concatenated C string literals between the
// "=" after name and the next ";".
func parseCStringAfter(s string, name string) ([]byte, error) {
	i := strings.Index(s, name)
	if i < 0 {
		return nil, fmt.Errorf("could not find %q", name)
	}
	s = s[i+len(name):]
	i = strings.IndexByte(s, '=')
	if i < 0 {
		return nil, fmt.Errorf("could not find %q's '='", name)
	}
	s = s[i+1:]
	ret := []byte(nil)
	for {
		s = strings.TrimLeft(s, " \t\r\n")
		if s == "" {
			return nil, fmt.Errorf("%s: unterminated", name)
		} else if s[0] == ';' {
			return ret, nil
		} else if s[0] != '"' {
			return nil, fmt.Errorf("%s: unexpected %q", name, s[0])
		}
		s = s[1:]
		for {
			if s == "" {
				return nil, fmt.Errorf("%s: unterminated string literal", name)
			}
			c := s[0]
			if c == '"' {
				s = s[1:]
				break
			} else if c != '\\' {
				ret = append(ret, c)
				s = s[1:]
				continue
			}
			if len(s) < 2 {
				return nil, fmt.Errorf("%s: bad escape", name)
			}
			c, s = s[1], s[2:]
			switch c {
			case 'n':
				ret = append(ret, '\n')
			case 't':
				ret = append(ret, '\t')
			case 'r':
				ret = append(ret, '\r')
			case '"', '\'', '\\', '?':
				ret = append(ret, c)
			case 'x':
				n := 0
				for n < len(s) && isHexDigit(s[n]) {
					n++
				}
				v, err := strconv.ParseUint(s[:n], 16, 8)
				if (n == 0) || (err != nil) {
					return nil, fmt.Errorf("%s: bad hex escape", name)
				}
				ret = append(ret, byte(v))
				s = s[n:]
			case '0', '1', '2', '3', '4', '5', '6', '7':
				n := 0
				for n < len(s) && n < 2 && ('0' <= s[n]) && (s[n] <= '7') {
					n++
				}
				v, err := strconv.ParseUint(string(c)+s[:n], 8, 8)
				if err != nil {
					return nil, fmt.Errorf("%s: bad octal escape", name)
				}
				ret = append(ret, byte(v))
				s = s[n:]
			default:
				return nil, fmt.Errorf("%s: unsupported escape %q", name, c)
			}
		}
	}
}

func isHexDigit(c byte) bool {
	return ('0' <= c && c <= '9') || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F')
}

const header = `// Copyright 2026 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

`
