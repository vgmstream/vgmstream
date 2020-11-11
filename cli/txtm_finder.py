import os
import struct

mpfTable=dict()

for fname in os.listdir(os.getcwd()):
    if fname.lower()[-4:] not in [".map",".mpf"]:
        continue

    print(fname)
    f=open(fname,"rb")
    magic=f.read(4)
    if magic!=b"PFDx": raise Exception("Wrong header magic in %s" % (f.name))
    version,introNode,numNodes,numSections,b1,b2,b3,numEvents=struct.unpack("8B",f.read(8))
    if version>1: raise Exception("Unsupported version %d in %s" % (version,f.name))
    f.seek(numNodes*0x1c,1)
    f.seek(numEvents*numSections,1)
    samplesTable=f.tell()
    
    for musName in os.listdir(os.getcwd()):
        if musName.lower()[-4:] not in [".mus",".trm",".trj"]:
            continue

        f.seek(samplesTable)
        f2=open(musName,"rb")
        passed=True
        for i in range(numNodes):
            offset=struct.unpack(">I",f.read(4))[0]
            f2.seek(offset)
            magic=f2.read(4)
            if magic!=b"SCHl":
                passed=False
                break

        f2.close()
        if passed:
            mpfTable[fname]=musName
            break

    f.close()

f=open(".txtm","w")
for mpf in mpfTable:
    f.write("%s:%s\n" % (mpf,mpfTable[mpf]))
f.close()
