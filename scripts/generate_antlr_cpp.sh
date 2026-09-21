#!/usr/bin/env sh

set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
temp_dir=$(mktemp -d)
trap 'rm -rf "$temp_dir"' EXIT HUP INT TERM

if [ -n "${ANTLR4_JAR:-}" ]; then
    antlr4_jar=$ANTLR4_JAR
elif [ -f /opt/homebrew/opt/antlr/antlr-4.13.2-complete.jar ]; then
    antlr4_jar=/opt/homebrew/opt/antlr/antlr-4.13.2-complete.jar
elif [ -f /usr/share/java/antlr4.jar ]; then
    antlr4_jar=/usr/share/java/antlr4.jar
else
    echo "ANTLR 4 tool jar not found; set ANTLR4_JAR=/path/to/antlr-4.x-complete.jar." >&2
    exit 1
fi

mkdir -p "$temp_dir/grammar" "$temp_dir/generated"
cp "$repo_root/grammar/Lexer.g4" "$temp_dir/grammar/RxLexer.g4"
cp "$repo_root/grammar/Parser.g4" "$temp_dir/grammar/RxParser.g4"
sed -i.bak 's/^lexer grammar Lexer;/lexer grammar RxLexer;/' "$temp_dir/grammar/RxLexer.g4"
sed -i.bak 's/^parser grammar Parser;/parser grammar RxParser;/' "$temp_dir/grammar/RxParser.g4"
sed -i.bak 's/tokenVocab=Lexer/tokenVocab=RxLexer/' "$temp_dir/grammar/RxParser.g4"
rm -f "$temp_dir"/grammar/*.bak

java -jar "$antlr4_jar" -Dlanguage=Cpp -visitor -no-listener \
    -o "$temp_dir/generated" "$temp_dir/grammar/RxLexer.g4"
java -jar "$antlr4_jar" -Dlanguage=Cpp -visitor -no-listener \
    -lib "$temp_dir/generated" -o "$temp_dir/generated" \
    "$temp_dir/grammar/RxParser.g4"

sed -i.bak "s|// Generated from $temp_dir/grammar/RxLexer.g4 by ANTLR 4.13.2|// Generated from grammar/Lexer.g4 by ANTLR 4.13.2|" "$temp_dir"/generated/*.cpp "$temp_dir"/generated/*.h
sed -i.bak "s|// Generated from $temp_dir/grammar/RxParser.g4 by ANTLR 4.13.2|// Generated from grammar/Parser.g4 by ANTLR 4.13.2|" "$temp_dir"/generated/*.cpp "$temp_dir"/generated/*.h
rm -f "$temp_dir"/generated/*.bak

mkdir -p "$repo_root/src/generated"
cp "$temp_dir/generated"/*.cpp "$temp_dir/generated"/*.h "$repo_root/src/generated/"
