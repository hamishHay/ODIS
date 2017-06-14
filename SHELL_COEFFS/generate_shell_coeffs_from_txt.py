import numpy as np

hs = np.arange(1,100+1,1)
l = np.arange(0,30,1)

input_beta_txt = "ENCELADUS/beta_hocean10_lmax30_Temax100.csv"
input_nu_txt = "ENCELADUS/nu_hocean10_lmax30_Temax100.csv"

beta_save = "ENCELADUS/beta_hs1km_to_hs100km_lmax30.txt"
nu_save = "ENCELADUS/nu_hs1km_to_hs100km_l2.txt"

beta_data = np.loadtxt(input_beta_txt,delimiter = ',')
nu_data = np.loadtxt(input_nu_txt,delimiter = ',')

save_file = open(beta_save, 'w')
for i in l:
    for j in hs:
        save_file.write("{:.10f}\t".format(beta_data[i][j-1]))
    save_file.write("\n")

save_file.close()

save_file = open(nu_save, 'w')
for j in hs:
    save_file.write("{:.10f}\t".format(nu_data[1][j-1]))
save_file.write("\n")
save_file.close()
