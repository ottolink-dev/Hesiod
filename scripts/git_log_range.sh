#!/bin/bash

# Usage: ./git_log_range.sh <commit-hash>
# Example: ./git_log_range.sh a26d14

# git log --since="1 week ago" --oneline

if [ -z "$1" ]; then
    echo "Usage: $0 <commit-hash>"
    exit 1
fi

hash="$1"

git log --oneline "${hash}^.."
