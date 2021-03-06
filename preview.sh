#!/bin/bash

# Load configuration overrides if defined
if [ -f config.sh ]; then source ./config.sh; fi

# Open a new copy of ds9 if it is not available, otherwise preview
if [ "$(xpaaccess -n Online_Preview)" = 0 ]; then
	./startup.sh
fi

# Move preview to a temporary file to try and avoid file locking problems under windows
mv preview.fits.gz preview.temp.fits.gz

# Define e.g. GUIDE_OUTPUT="$(pwd)/guide.pos" in config.sh to enable guide output
tsreduce preview $(pwd)/preview.temp.fits.gz Online_Preview ${GUIDE_OUTPUT}
rm preview.temp.fits.gz