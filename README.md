[![DOI](https://zenodo.org/badge/170377406.svg)](https://zenodo.org/badge/latestdoi/170377406)
# TimePix3DosimetricLibrary
A basic library for dosimetric measurements with a TimePix3 detector

# Usage
Select in cmake whether you want to use a real katherine readout unit for data acquisition or simulate it via a file called SampleData.t3pa in the binary directory. The SampleData.t3pa can be the output of the katherine pixman or the ADVACAM tpx3 program.
The main object needed to start a measurement and register for events is the Tpx3DosageMeasurement object.

# Events
In order to get the calculated results correctly please register for one of the events provided by the PostProcessing thread.


# Submodules
The "katherine" submodule is pointing a branch of the master thesis of Petr Mánek from the university of Prag.
