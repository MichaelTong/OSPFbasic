OBJ = Interface.o InterfaceFSM.o LSA.o LsaList.o main.o Neighbor.o \
	NeighborFSM.o netLSA.o ospfAck.o ospfCalc.o OSPF.o ospfDD.o \
	ospfHello.o ospfOrig.o ospfRequest.o ospfUpdate.o ospfUtil.o \
	Patricia.o phyint.o Queue.o rtrLSA.o RtrTblEntry.o system.o \
	Table.o Timer.o
HEAD = IP.h arch.h define.h system.h ospfPkt.h include.h ospfType.h
THOSPF : $(OBJ)
	g++ -o THOSPF $(OBJ)
$(OBJ) : $(HEAD)
clean : 
	rm $(OBJ)

