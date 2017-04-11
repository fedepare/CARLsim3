#########
# IMPORTS
#########

###########
# FUNCTIONS
###########


######
# MAIN
######

cnt  = 0
cnt2 = 0
for i in xrange(0, 32):
	for j in xrange(0, 32):
		if i != 0 and i!= 32-1 and j != 0 and j!= 32-1:
			if cnt % 2 != 0:
				cnt2 += 1
		cnt += 1

print cnt2