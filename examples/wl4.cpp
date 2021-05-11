
#include "QuEST.h"
#include "mytimer.hpp"
#include "utils.cpp"
#include <assert.h>
#include <stdio.h>

int main() {

  FILE *fp = fopen("ExpHam.dat", "w");
  if (fp == NULL) {
    printf("    open probs.dat failed, Bye!");
    return 0;
  }

  int numQubits = 22;
  vector<Ham_term> Ham_terms = parse_Ham("ham_H12.dat");

  QuESTEnv env = createQuESTEnv();
  Qureg QReg = createQureg(numQubits, env);
  initZeroState(QReg);

  /* start timing */
  double t0 = get_wall_time();

/* add ansztz circuit */
#include "ansatz_circuit.dat"

  Qureg QReg2 = createQureg(numQubits, env);

  vector<double> Energies;
//   printf("Size : %lld\n", Ham_terms.size());
  int sum = 0;
  for (const auto &ham : Ham_terms) {
    cloneQureg(QReg2, QReg);
    int QInd = -1;
//     sum++;
//     if (sum == 100){
//       printf("x");
//       sum = 0;
//   }
    // if (ham.second.size() != 22) printf(" AAze : %lld\n", ham.second.size());
    for(const auto &x: ham.second){
        QInd++;
        if(x=='I') continue;
        else if(x=='X') pauliX(QReg2, QInd);
        else if(x=='Y') pauliY(QReg2, QInd);
        else if(x=='Z') pauliZ(QReg2, QInd);
    }
    Energies.push_back(ham.first * InnerProduct(QReg, QReg2));
  }

  double Energy = 0;
  int cnt = 0;
  for (auto en : Energies) {
    fprintf(fp, "Expectation value of %4d th Hamiltonian: %12.6lf\n", cnt++,
            en);
    Energy += en;
  }
  printf("Calculated energy for given Hamiltonian is %12.6lf\n", Energy);

  /* finish timing */
  double t = get_wall_time() - t0;
  printf(" Time cost: %lf\n", t);

  destroyQureg(QReg, env);
  destroyQuESTEnv(env);

  return 0;
}
