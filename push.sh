#!/bin/bash

# Prompt the user for a commit message
echo -n "Enter commit message: "
read msg

# Check if message is empty
if [[ -z "$msg" ]]; then
  echo "❌ Commit message cannot be empty!"
  exit 1
fi

# Add all changes
git add .

# Commit
git commit -m "$msg"

# Get current branch name
branch=$(git rev-parse --abbrev-ref HEAD)

# Push to current branch
git push origin "$branch"
