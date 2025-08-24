#!/bin/bash

# Ensure folder is given
if [ -z "$1" ]; then
    echo "Usage: $0 <folder_path>"
    exit 1
fi

FOLDER="$1"

# Check ctags
if ! command -v ctags >/dev/null; then
    echo "Error: ctags is not installed."
    exit 1
fi

# Use a temp file
TMP_TAGS_FILE=$(mktemp)

# Generate tags with return type and signature
ctags --fields=+S+t --c-kinds=f --output-format=xref -o "$TMP_TAGS_FILE" $(find "$FOLDER" -name '*.c')

# Extract and print functions with signatures
while read -r line; do
    file=$(echo "$line" | awk '{print $NF}')
    name=$(echo "$line" | awk '{print $1}')
    ret=$(echo "$line" | grep -o 'typeref:.*' | sed -E 's/typeref:[^ ]+ //;s/ signature:.*//')
    sig=$(echo "$line" | sed -n 's/.*signature:\(.*\)/\1/p')
    echo "$file: $ret $name($sig)" | sed 's/  */ /g' | sed 's/ *( */(/'
done < "$TMP_TAGS_FILE"

# Clean up
rm "$TMP_TAGS_FILE"

