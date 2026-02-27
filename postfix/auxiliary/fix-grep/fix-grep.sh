#!/bin/sh

# Fix grep -[EF] for systems that require the historical forms egrep
# and fgrep. Run this script from the top-level Postfix directory as
#     sh auxiliary/fix-grep/fix-grep.sh

# Use only historical grep syntax.
find * -type f | xargs grep -l 'grep -[EF]' | xargs perl -pi -e '
	s/grep -E/egrep/g;
	s/grep -F/fgrep/g;
'
