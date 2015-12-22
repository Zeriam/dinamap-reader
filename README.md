# dinamap-reader
Dinamap Pro 1000 Native Binary Mode Data Reader and Waveform Plotter

The data file, dinamap_data.txt, contains output from a Dinamap Pro 1000 running in Native Binary Mode.  The libplot library from GNU plotutils is used to draw the EKG waveform. Specifically, an X plotter is utilized, which requires the X Window System.  The program reads the waveform data and plots it, simulating the 50 Hz timing of an actual device.  Additionally, it detects and reports QRS events, breaths, warnings alarms, and crisis alarms.

Compile with `gcc -lplot -Wall -Wextra -o dinamap_reader dinamap_reader.c`



