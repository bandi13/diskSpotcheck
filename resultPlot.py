import numpy as np
import matplotlib
matplotlib.use('Agg')
#matplotlib.ioff()
import matplotlib.pyplot as plt
import csv
import sys
import os

#pdf plot would be saved in a folder named resultPlot
#if this folder is not exist, the script would genrate the directory
script_dir = os.path.dirname(__file__)
results_dir = os.path.join(script_dir, 'fileSystemPlot/')

if not os.path.isdir(results_dir):
    os.makedirs(results_dir)


with open(sys.argv[1], 'rb') as f:
    reader = csv.reader(f)
    fs = sys.argv[2]
    file = sys.argv[3]
#    print fs, file
    data = []
    xtick = ["w"]
    count = 0
    for row in reader:
        if(row[0]==fs and row[1].split('/').pop()==file):
            data.append(float(row[3]))
            if(count>0): xtick.append("r"+str(count))
            count+=1

ind = np.arange(count)
width = 0.35

p1 = plt.bar(ind, data, width)
plt.title('write/read test of '+file+' in '+fs)
plt.ylabel('time (s)')
plt.xlabel('index of w/r')
plt.xticks(ind + width/2., xtick)
plt.autoscale(enable=True, axis='both', tight=None)
#plt.savefig("/resultPlot/"+sys.argv[1][:-4]+"_"+fs+"_"+file+".pdf")
outImage = results_dir+os.path.splitext(os.path.basename(sys.argv[1]))[0]+"_"+fs+"_"+file+".svg"
#print outImage
plt.savefig(outImage)
#plt.show()
