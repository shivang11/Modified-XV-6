import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches

x=[]
y=[]
z=[]
pid4x=[]
pid4y=[]
pid5x=[]
pid5y=[]
pid6x=[]
pid6y=[]
pid7x=[]
pid7y=[]
pid8x=[]
pid8y=[]

x,y,z = np.loadtxt('data.txt',delimiter=',',unpack=True)


i=0
while i<len(x):
    if (x[i]==4):
        pid4x.append(int(y[i]))
        pid4y.append(int(z[i]))
    elif(x[i]==5):
        pid5x.append(int(y[i]))
        pid5y.append(int(z[i]))
    elif(x[i]==6):
        pid6x.append(int(y[i]))
        pid6y.append(int(z[i]))
    elif(x[i]==7):
        pid7x.append(int(y[i]))
        pid7y.append(int(z[i]))
    elif(x[i]==8):
        pid8x.append(int(y[i]))
        pid8y.append(int(z[i]))
    i+=1


plt.plot(pid4x,pid4y,color="green")
plt.plot(pid5x,pid5y,color="yellow")
plt.plot(pid6x,pid6y,color="red")
plt.plot(pid7x,pid7y,color="purple")
plt.plot(pid8x,pid8y,color="orange")

g_p=mpatches.Patch(color="green",label="pid=4")
y_p=mpatches.Patch(color="yellow",label="pid=5")
r_p=mpatches.Patch(color="red",label="pid=6")
p_p=mpatches.Patch(color="purple",label="pid=7")
o_p=mpatches.Patch(color="orange",label="pid=8")
plt.legend(handles=[g_p,y_p,r_p,p_p,o_p])


plt.xlabel('no. of ticks')
plt.ylabel('queue_no')

plt.grid(True)

plt.show()

