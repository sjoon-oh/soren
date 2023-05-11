# Mu: us Consensus for us Applications
# HERD-Mu Integration, Lines
# Author: SukJoon Oh, sjoon@kaist.ac.kr

# Frequently used keywords
# httpointsize://hirophysics.com/gnuplot/gnuplot08.html

reset session

# Set the necessaries
DATA_DIR="./"

DATA_FNAME1="summary-gnuplot.dat"


EXPORT_DIR="./"
EXPORT_NAME="soren-st-lat"

# Export path
set output EXPORT_DIR.EXPORT_NAME.".eps"

# New setting
FONT_GLOBAL="Times-Roman,28"
FONT_TITLE="Times-Roman,28"
FONT_XLABEL="Times-Roman,28"
FONT_YLABEL="Times-Roman,28"
FONT_XTICS="Times-Roman,22"
FONT_YTICS="Times-Roman,22"
FONT_KEY="Times-Roman,28"

#
# Data specifications 
TITLE=""

XLABEL="# of Client Threads"
YLABEL="Latency (us)"

#
# Settings starts here.
# Overall graph structure settings lies here.
# ----

# set terminal postscript eps \
#     enhanced monochrome 28 \
#     font FONT_GLOBAL

set terminal postscript eps \
    color \
    font FONT_GLOBAL

set title TITLE font FONT_TITLE
unset title

set xlabel XLABEL font FONT_XLABEL
set ylabel YLABEL font FONT_YLABEL

set xtics nomirror
set ytics nomirror

# 
set grid ytics

# set key on \
#     bottom \
#     noautotitle \
#     font FONT_KEY

set key reverse \
    samplen 2 width 0 \
    left top \
    Left nobox font FONT_KEY

# 
# Set your options: outside, inside, bottom, ...

set style line 1 linecolor rgb "red"     linetype 1 linewidth 3.5 pointtype 1 pointsize 1.5 pi -1  ## +
set style line 2 linecolor rgb "blue"    linetype 2 linewidth 3.5 pointtype 2 pointsize 1.5 pi -1  ## x
set style line 3 linecolor rgb "#00CC00" linetype 1 linewidth 3.5 pointtype 3 pointsize 1.5 pi -1  ## *
set style line 4 linecolor rgb "#7F171F" linetype 4 linewidth 3.5 pointtype 4 pointsize 1.5 pi -1  ## box
set style line 5 linecolor rgb "#FFD800" linetype 3 linewidth 3.5 pointtype 5 pointsize 1.5 pi -1  ## solid box
set style line 6 linecolor rgb "#000078" linetype 6 linewidth 3.5 pointtype 6 pointsize 1.5 pi -1  ## circle
set style line 7 linecolor rgb "#732C7B" linetype 7 linewidth 3.5 pointtype 7 pointsize 1.5 pi -1
set style line 8 linecolor rgb "black"   linetype 8 linewidth 3.5 pointtype 8 pointsize 1.5 pi -1  ## triangle

# set style line 1 linecolor rgb "black"    linetype 1 linewidth 2 pointtype 1 pointsize 1.5 pi -1  ## +
# set style line 2 linecolor rgb "black"    linetype 2 linewidth 2 pointtype 2 pointsize 1.5 pi -1  ## x
# set style line 3 linecolor rgb "black"    linetype 1 linewidth 2 pointtype 3 pointsize 1.5 pi -1  ## *
# set style line 4 linecolor rgb "black"    linetype 4 linewidth 2 pointtype 4 pointsize 1.5 pi -1  ## box
# set style line 5 linecolor rgb "black"    linetype 3 linewidth 2 pointtype 5 pointsize 1.5 pi -1  ## solid box
# set style line 6 linecolor rgb "black"    linetype 6 linewidth 2 pointtype 6 pointsize 1.5 pi -1  ## circle
# set style line 7 linecolor rgb "black"    linetype 7 linewidth 2 pointtype 7 pointsize 1.5 pi -1
# set style line 8 linecolor rgb "black"    linetype 8 linewidth 2 pointtype 8 pointsize 1.5 pi -1  ## triangle

#
# Write plots here
# ----
set style fill pattern
set style histogram clustered gap 2 errorbars

# set xrange[0:36]
# set ytics 20
set yrange[0:]
# set ytics 200

plot \
    DATA_DIR.DATA_FNAME1 using 2:3:4 title "node0" linewidth 3 linetype -1 with histogram, \
    # DATA_DIR.DATA_FNAME1 using 2 title "node0" linewidth 3 linetype -1 with yerrorb
# i 0 u ($0-bs):1:($2**2) notitle w yerrorb ls 1, \

# DATA_DIR.DATA_FNAME1 using ($1 + 1):($2 + 1):2 with labels font "Times-Roman,20" textcolor rgb "red" notitle , \