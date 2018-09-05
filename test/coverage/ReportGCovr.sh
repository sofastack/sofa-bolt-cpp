#!/usr/bin
rm -f *.html
gcovr -r ../../ --html  --gcov-exclude='.*(?:Logger|pb|unit_test|example).*' --html-details -o detail.html
gcovr -r ../../ --html  --gcov-exclude='.*(?:Logger|pb|unit_test|example).*' -o summary.html
