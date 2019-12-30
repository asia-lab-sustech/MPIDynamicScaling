/* 
 *  Copyright (c) 2019 Masatoshi Hanai
 *
 *  This software is released under MIT License.
 *  See LICENSE.
 *
 */

#ifndef DYNAMIC_SCALING_HPP
#define DYNAMIC_SCALING_HPP

#include <cstdio>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <mpi.h>

class DynamicScaler {
 public:
  bool LOG_;
  void scaleOut(const MPI_Comm& oldCommunicator, int numAddProcesses, const std::string& childProcessCommand,
      MPI_Comm& newCommunicator, const std::vector<std::string>& hosts = std::vector<std::string>()) {
    int parentRank = 0;
    int rank = 0;
    MPI_Comm_rank(oldCommunicator, &rank);
    MPI_Comm intracomToChildren = MPI_COMM_NULL;
    int changeRank = false;
    MPI_Comm intercomToChildren;
    if (!std::ifstream(childProcessCommand).good()) {
      std::cerr << "<<<< Error: Execution File does not exist >>>>" << std::endl;
      std::cerr << "<<<<   -- File Name: " << childProcessCommand << ">>>>" << std::endl;
    };

    MPI_Info info = MPI_INFO_NULL;
    int numHosts = hosts.size();
    std::string pathAddHost;
    if (numHosts != 0) {
      if (rank == 0) {
        auto current = [] {
          time_t now = time(0);
          struct tm tstruct;
          char buf[80];
          tstruct = *localtime(&now);
          strftime(buf, sizeof(buf), "%Y-%m-%d-%X", &tstruct);
          return std::string(buf);
        };
        pathAddHost = "add-hosts-" + current();
        std::ofstream outfile(pathAddHost);
        for (int h = 0; h < numHosts; ++h) {
          outfile << hosts[h] << std::endl;
        }
        outfile.close();
        MPI_Info_create(&info);
        MPI_Info_set(info, "add-hostfile", &pathAddHost[0]);
      }
    }
    MPI_Barrier(oldCommunicator);

    double start = MPI_Wtime();
    MPI_Comm_spawn(&childProcessCommand[0],
                   MPI_ARGV_NULL,
                   numAddProcesses,
                   info,
                   parentRank,
                   oldCommunicator,
                   &intercomToChildren,
                   NULL);
    double end = MPI_Wtime();
    if (LOG_) std::cout << "!!!! Parent: Create " << numAddProcesses << " child process(es) from rank " << parentRank << ". Time to create " << (end - start) << " (sec) !!!!" << std::endl;
    MPI_Intercomm_merge(intercomToChildren, changeRank, &newCommunicator);
    if (LOG_) std::cout << "!!!! Parent: Establish IntraComm to Children !!!!" << std::endl;
    MPI_Comm_free(&intercomToChildren);
    if (info != MPI_INFO_NULL) {
      MPI_Info_free(&info);
      if (rank == 0) std::remove(&pathAddHost[0]);
    }
  };

  /* This function needs to be called when the new processes is invoked */
  void initNewProcess(MPI_Comm& newComm) {
    int mpiInit = false;
    MPI_Initialized(&mpiInit);
    if (!mpiInit) {
      int SUPPORT = 0;
      MPI_Init_thread(NULL,NULL,MPI_THREAD_MULTIPLE,&SUPPORT);
    }
    MPI_Comm commWorldAmongChildren = MPI_COMM_WORLD;
    int childrenSize, childrenRank;
    MPI_Comm_size(commWorldAmongChildren, &childrenSize);
    MPI_Comm_rank(commWorldAmongChildren, &childrenRank);

    if (LOG_) {
      char myhostname[256]; gethostname(myhostname, sizeof (myhostname));
      std::cout << "!!!! -- Child " << childrenRank << ": Child process #" << childrenRank << " is launched at " << myhostname << " !!!!" << std::endl;
    }

    MPI_Comm interCommToParent;
    MPI_Comm_get_parent(&interCommToParent);
    MPI_Comm intracomToParent;
    int changeRank = true;
    MPI_Intercomm_merge(interCommToParent, changeRank, &newComm);
  };

  void scaleIn(MPI_Comm& oldCommunicator, bool isRemove, MPI_Comm& newCommunicator) {
    bool tmp;
    scaleIn(oldCommunicator, isRemove, newCommunicator, tmp);
  }

  void scaleIn(MPI_Comm& oldCommunicator, bool isRemove, MPI_Comm& newCommunicator, bool& thisHostCanBeRemoved) {
    MPI_Comm parentComm = MPI_COMM_NULL;
    MPI_Comm_get_parent(&parentComm);
    int rank, size;
    MPI_Comm_rank(oldCommunicator, &rank);
    MPI_Comm_size(oldCommunicator, &size);

    if (parentComm == MPI_COMM_NULL && isRemove) {
      std::cerr << "<<<< Error at " << __LINE__ << " in " << __FILE__ << std::endl;
      std::cerr << "<<<< The original process cannot be removed." << std::endl;
      std::cerr << "<<<< This process (Rank " << rank << ") is the original process" << std::endl;
      std::cerr << "<<<< Only the added processes can be removed." << std::endl;
      exit(1);
    }

    MPI_Comm_split(oldCommunicator, isRemove, rank, &newCommunicator);

    /* Check other MPI processes appear in this host */
    const int charSize = 32;
    char remainingHostname[charSize];
    if (!isRemove) gethostname(remainingHostname, sizeof(remainingHostname));
    std::string remainingHosts = std::string(charSize*size, 'N');
    MPI_Allgather(remainingHostname, charSize, MPI_CHAR, &remainingHosts[0], charSize, MPI_CHAR, oldCommunicator);
    thisHostCanBeRemoved = true;
    char myhostname[charSize];
    gethostname(myhostname, sizeof(myhostname));
    for (int r = 0; r < size; ++r) {
      if (strncmp(&remainingHosts[r*charSize], myhostname, charSize) == 0) thisHostCanBeRemoved = false;
    }

    if (LOG_ && rank == 0) {
      int newSize;
      MPI_Comm_size(newCommunicator, &newSize);
      std::cout << "!!!! ScaleIn from " << size << " to " << newSize << " !!!!" << std::endl;
    }

    if (isRemove) {
      /* remove process */
      if (LOG_) {
        gethostname(myhostname, sizeof(myhostname));
        std::cout << "!!!! -- Process " << rank << " is killed at " << myhostname << " !!!!" << std::endl;
      }
      MPI_Comm_free(&oldCommunicator);
      MPI_Comm_free(&newCommunicator);
      MPI_Finalize();
      return;
    }
  };

 public:
  DynamicScaler(): LOG_(false) {};
  ~DynamicScaler(){};
};

#endif //DYNAMIC_SCALING_HPP

