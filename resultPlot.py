import numpy as np
import matplotlib.pyplot as plt
import csv
import sys

with open('/Users/guanlai/Desktop/testResult.csv', 'rb') as f:
    reader = csv.reader(f)
    fs = sys.argv[1]
    file = sys.argv[2]
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
plt.savefig(fs+"_"+file+"_test_result.pdf")
plt.show()
