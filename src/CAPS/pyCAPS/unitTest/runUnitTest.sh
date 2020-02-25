#!/bin/bash

python --version
python -m unittest discover

# Fix cleanup
if [ -d "__pycache__" ]; then
    rm -r __pycache__
else
    for filename in ./*.pyc; do
        rm $filename
    done
fi
