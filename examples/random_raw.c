// //total number of qubit: 30
// //total number of qubit operatations: 667
// //estimated time: 3783.9266747315614 second.
// #include "QuEST.h"
// #include "stdio.h"
// #include "mytimer.hpp"


// int main (int narg, char *argv[]) {

//     QuESTEnv Env = createQuESTEnv();
//     double t1 = get_wall_time();

//     FILE *fp=fopen("probs.dat", "w");
//     if(fp==NULL){
//         printf("    open probs.dat failed, Bye!");
//         return 0;
//     }

//     FILE *fvec=fopen("stateVector.dat", "w");
//     if(fp==NULL){
//         printf("    open stateVector.dat failed, Bye!");
//         return 0;
//     }

//     Qureg q = createQureg(30, Env);

//     float q_measure[30];
//     tGate(q, 25);
//     controlledNot(q, 28, 21);
//     controlledRotateX(q, 17, 5, 0.3293660327520663);
//     rotateX(q, 27, 3.9207427542347926);
//     tGate(q, 3);
//     controlledRotateZ(q, 27, 19, 5.459935259485407);
//     controlledRotateX(q, 26, 3, 4.736990305652013);
//     controlledRotateZ(q, 8, 11, 3.594728080156504);
//     rotateX(q, 10, 4.734238389048838);
//     rotateY(q, 8, 4.959946047271496);
//     rotateZ(q, 5, 1.0427019597472071);
//     controlledRotateZ(q, 27, 0, 5.971846444908922);
//     pauliZ(q, 0);
//     tGate(q, 4);
//     controlledRotateX(q, 29, 17, 5.885491371058282);
//     tGate(q, 6);
//     tGate(q, 23);
//     controlledRotateZ(q, 28, 11, 4.12817017175927);
//     hadamard(q, 17);
//     controlledNot(q, 17, 3);
//     rotateZ(q, 0, 3.8932024879670144);
//     controlledRotateY(q, 22, 28, 5.384534074265311);
//     controlledNot(q, 29, 5);
//     sGate(q, 8);
//     controlledPauliY(q, 23, 14);
//     controlledPauliY(q, 18, 17);
//     controlledPauliY(q, 9, 15);
//     pauliY(q, 6);
//     controlledNot(q, 19, 29);
//     controlledPauliY(q, 1, 25);
//     pauliZ(q, 8);
//     pauliY(q, 2);
//     pauliX(q, 9);
//     controlledPauliY(q, 4, 12);
//     controlledRotateY(q, 17, 14, 4.308551563407819);
//     rotateX(q, 11, 5.512541996174936);
//     pauliX(q, 24);
//     pauliY(q, 7);
//     tGate(q, 18);
//     hadamard(q, 27);
//     pauliZ(q, 29);
//     controlledNot(q, 15, 13);
//     controlledRotateZ(q, 16, 3, 1.375780109278382);
//     pauliZ(q, 28);
//     controlledRotateX(q, 23, 20, 4.5063180242513505);
//     pauliZ(q, 21);
//     sGate(q, 6);
//     rotateX(q, 18, 2.337847412996701);
//     tGate(q, 21);
//     rotateY(q, 21, 2.5090791677412008);
//     controlledRotateY(q, 13, 8, 2.5004731956129143);
//     controlledPauliY(q, 2, 10);
//     controlledNot(q, 22, 1);
//     pauliX(q, 3);
//     pauliX(q, 3);
//     rotateX(q, 29, 2.723779839507815);
//     hadamard(q, 2);
//     controlledRotateZ(q, 29, 8, 3.7348369237156893);
//     controlledPauliY(q, 17, 10);
//     pauliY(q, 13);
//     sGate(q, 13);
//     controlledRotateY(q, 21, 4, 1.0006054937741466);
//     tGate(q, 12);
//     controlledPauliY(q, 14, 4);
//     pauliZ(q, 11);
//     controlledPauliY(q, 13, 4);
//     controlledNot(q, 18, 4);
//     rotateX(q, 27, 5.456179791071725);
//     rotateY(q, 23, 2.3597295726584417);
//     pauliY(q, 18);
//     rotateX(q, 20, 4.663082879319556);
//     controlledRotateY(q, 17, 3, 3.379870011915129);
//     pauliZ(q, 17);
//     controlledRotateY(q, 27, 8, 4.729823556797339);
//     rotateY(q, 10, 1.9665821442518263);
//     hadamard(q, 21);
//     hadamard(q, 23);
//     pauliY(q, 1);
//     hadamard(q, 20);
//     pauliX(q, 19);
//     rotateZ(q, 14, 2.0069208879155003);
//     sGate(q, 17);
//     rotateY(q, 7, 1.1987039711422482);
//     controlledRotateY(q, 16, 25, 5.525016274711897);
//     pauliZ(q, 2);
//     pauliY(q, 19);
//     controlledRotateX(q, 5, 22, 5.474489446026321);
//     controlledRotateZ(q, 22, 25, 2.054682600662274);
//     controlledPauliY(q, 19, 6);
//     tGate(q, 14);
//     rotateY(q, 25, 5.689131875569378);
//     rotateY(q, 29, 5.261268123984145);
//     rotateY(q, 18, 5.340898512406205);
//     controlledRotateY(q, 5, 8, 0.2087337909838518);
//     tGate(q, 7);
//     pauliY(q, 2);
//     controlledNot(q, 26, 12);
//     controlledRotateX(q, 27, 15, 5.113996985399576);
//     hadamard(q, 20);
//     pauliZ(q, 8);
//     tGate(q, 10);
//     hadamard(q, 9);
//     pauliZ(q, 8);
//     rotateY(q, 21, 5.899576921821051);
//     pauliY(q, 24);
//     controlledRotateZ(q, 11, 23, 1.1916005322627135);
//     controlledRotateZ(q, 18, 7, 2.558871283621717);
//     pauliX(q, 15);
//     hadamard(q, 23);
//     rotateX(q, 10, 5.259645311585795);
//     controlledNot(q, 19, 26);
//     rotateY(q, 18, 5.982090815955244);
//     controlledRotateX(q, 10, 26, 1.9709969724073322);
//     tGate(q, 22);
//     hadamard(q, 20);
//     controlledRotateX(q, 6, 12, 0.8115870637427451);
//     controlledRotateZ(q, 22, 7, 2.0426711293536624);
//     hadamard(q, 22);
//     rotateY(q, 17, 5.853295168431142);
//     controlledPauliY(q, 12, 22);
//     rotateZ(q, 7, 2.8526004701547407);
//     controlledRotateY(q, 25, 23, 2.558389682561494);
//     pauliZ(q, 12);
//     pauliZ(q, 3);
//     pauliZ(q, 11);
//     controlledRotateY(q, 19, 26, 1.277492888938793);
//     rotateY(q, 21, 0.060308190727150726);
//     pauliX(q, 28);
//     controlledRotateX(q, 28, 4, 4.43758129807702);
//     pauliY(q, 3);
//     rotateX(q, 28, 0.04237668668269988);
//     rotateY(q, 0, 3.2152228716496687);
//     rotateX(q, 6, 1.9522178220075468);
//     sGate(q, 21);
//     rotateX(q, 14, 0.14465903710247485);
//     rotateX(q, 10, 2.357466065814179);
//     controlledPauliY(q, 15, 22);
//     hadamard(q, 7);
//     hadamard(q, 28);
//     controlledPauliY(q, 17, 8);
//     rotateZ(q, 21, 5.805453237930793);
//     pauliX(q, 7);
//     rotateZ(q, 16, 3.203898556913182);
//     pauliZ(q, 6);
//     tGate(q, 4);
//     rotateZ(q, 6, 4.025815161559468);
//     tGate(q, 0);
//     pauliY(q, 29);
//     rotateX(q, 21, 5.370693097083298);
//     pauliX(q, 10);
//     sGate(q, 6);
//     controlledRotateY(q, 22, 10, 1.1887222106718032);
//     rotateY(q, 28, 5.199562602514604);
//     rotateZ(q, 8, 4.913636270066778);
//     tGate(q, 17);
//     controlledRotateY(q, 29, 1, 5.98773022967055);
//     sGate(q, 19);
//     pauliY(q, 0);
//     rotateZ(q, 13, 2.9212210982895233);
//     controlledRotateZ(q, 13, 29, 5.552571902471062);
//     controlledRotateY(q, 3, 19, 5.270894737271077);
//     rotateZ(q, 25, 2.908811731203406);
//     tGate(q, 7);
//     controlledRotateZ(q, 6, 3, 0.428281675897185);
//     rotateX(q, 24, 4.1882195109718685);
//     tGate(q, 10);
//     controlledPauliY(q, 10, 19);
//     rotateX(q, 21, 1.391966981880439);
//     pauliZ(q, 23);
//     pauliX(q, 2);
//     sGate(q, 14);
//     pauliZ(q, 13);
//     hadamard(q, 14);
//     hadamard(q, 23);
//     pauliY(q, 5);
//     pauliZ(q, 11);
//     hadamard(q, 15);
//     controlledRotateY(q, 22, 15, 2.19259006333866);
//     rotateX(q, 20, 1.2248470678913959);
//     controlledRotateX(q, 19, 20, 0.4168814783342205);
//     controlledRotateY(q, 29, 11, 2.1550188211826415);
//     controlledRotateX(q, 5, 8, 3.6526878685596724);
//     controlledRotateZ(q, 26, 6, 3.280905393538504);
//     controlledPauliY(q, 21, 25);
//     controlledRotateX(q, 2, 6, 4.631188238427102);
//     controlledPauliY(q, 16, 6);
//     pauliX(q, 10);
//     controlledRotateY(q, 8, 29, 1.255594460790626);
//     rotateY(q, 28, 5.031732637197544);
//     controlledPauliY(q, 18, 22);
//     pauliZ(q, 16);
//     controlledNot(q, 15, 25);
//     controlledRotateX(q, 26, 3, 3.490966266781222);
//     pauliY(q, 8);
//     controlledRotateZ(q, 8, 24, 4.228463856796449);
//     controlledRotateX(q, 29, 26, 4.998474967354304);
//     rotateX(q, 29, 1.1636964735443345);
//     rotateZ(q, 26, 2.3369723003783336);
//     rotateY(q, 13, 1.6810051810172748);
//     rotateZ(q, 15, 1.8075535002511007);
//     controlledRotateX(q, 5, 18, 2.416883664446432);
//     controlledRotateY(q, 18, 17, 0.648019671294397);
//     controlledRotateX(q, 9, 7, 3.493102984294659);
//     sGate(q, 23);
//     pauliZ(q, 22);
//     pauliY(q, 7);
//     rotateX(q, 20, 1.9954522247501965);
//     rotateY(q, 7, 2.434786533021047);
//     sGate(q, 28);
//     controlledRotateZ(q, 0, 11, 3.423783716636755);
//     rotateX(q, 5, 0.4093076042638878);
//     sGate(q, 10);
//     controlledNot(q, 6, 13);
//     controlledNot(q, 27, 19);
//     tGate(q, 29);
//     controlledRotateZ(q, 27, 8, 3.339292527760266);
//     sGate(q, 19);
//     pauliX(q, 21);
//     pauliZ(q, 22);
//     controlledRotateX(q, 1, 16, 2.025345832758928);
//     controlledRotateY(q, 22, 14, 1.48823399258975);
//     pauliY(q, 24);
//     pauliZ(q, 1);
//     tGate(q, 17);
//     pauliY(q, 20);
//     rotateZ(q, 0, 4.519820491695443);
//     hadamard(q, 14);
//     controlledNot(q, 14, 0);
//     controlledRotateZ(q, 24, 14, 3.131700953549889);
//     controlledNot(q, 17, 11);
//     pauliZ(q, 23);
//     controlledRotateX(q, 20, 14, 5.825881533762951);
//     rotateZ(q, 13, 3.2722790686170695);
//     controlledRotateZ(q, 21, 15, 0.399389573881667);
//     sGate(q, 9);
//     rotateX(q, 21, 0.31478293211138775);
//     controlledPauliY(q, 5, 24);
//     controlledRotateX(q, 12, 23, 1.0895046282787624);
//     controlledPauliY(q, 0, 28);
//     controlledPauliY(q, 27, 16);
//     pauliY(q, 16);
//     pauliX(q, 29);
//     rotateY(q, 6, 4.885633660532332);
//     rotateY(q, 24, 5.31079465664573);
//     pauliY(q, 22);
//     pauliZ(q, 4);
//     pauliX(q, 12);
//     pauliX(q, 2);
//     rotateX(q, 27, 1.0715903786570726);
//     hadamard(q, 28);
//     controlledRotateX(q, 21, 18, 4.551680290778682);
//     controlledRotateY(q, 4, 5, 2.7596083118671384);
//     pauliX(q, 23);
//     controlledRotateY(q, 11, 16, 5.827279333041572);
//     tGate(q, 17);
//     controlledRotateY(q, 12, 20, 3.737021201575457);
//     pauliY(q, 23);
//     pauliX(q, 9);
//     tGate(q, 20);
//     controlledRotateZ(q, 14, 16, 5.970243279785017);
//     controlledPauliY(q, 29, 13);
//     rotateY(q, 11, 2.0969213518198777);
//     pauliY(q, 27);
//     pauliZ(q, 20);
//     tGate(q, 21);
//     sGate(q, 28);
//     pauliZ(q, 4);
//     hadamard(q, 0);
//     controlledRotateY(q, 28, 16, 3.544201132024198);
//     controlledRotateY(q, 20, 5, 2.265689607162966);
//     controlledRotateZ(q, 14, 22, 5.623312222483167);
//     controlledRotateZ(q, 4, 27, 5.2853570136537185);
//     controlledNot(q, 19, 13);
//     pauliX(q, 27);
//     pauliY(q, 22);
//     controlledRotateY(q, 17, 29, 0.01515507024014199);
//     hadamard(q, 16);
//     controlledPauliY(q, 8, 17);
//     tGate(q, 27);
//     rotateY(q, 5, 6.271673132375288);
//     pauliZ(q, 18);
//     pauliY(q, 15);
//     tGate(q, 8);
//     controlledPauliY(q, 0, 3);
//     rotateZ(q, 22, 3.7355916443658193);
//     rotateY(q, 29, 3.361348364952901);
//     pauliZ(q, 8);
//     controlledNot(q, 10, 25);
//     rotateZ(q, 22, 1.993847181578224);
//     tGate(q, 4);
//     tGate(q, 4);
//     tGate(q, 16);
//     sGate(q, 7);
//     rotateZ(q, 3, 5.505909585130433);
//     controlledPauliY(q, 11, 27);
//     controlledRotateY(q, 10, 12, 4.797861533096972);
//     controlledPauliY(q, 26, 20);
//     pauliZ(q, 16);
//     pauliY(q, 19);
//     controlledRotateY(q, 25, 27, 0.07495365116740556);
//     controlledPauliY(q, 22, 23);
//     tGate(q, 22);
//     rotateZ(q, 22, 2.6352787752232842);
//     tGate(q, 12);
//     tGate(q, 13);
//     pauliY(q, 10);
//     controlledPauliY(q, 2, 13);
//     controlledRotateX(q, 21, 12, 4.706155994322334);
//     pauliX(q, 9);
//     controlledPauliY(q, 14, 1);
//     rotateX(q, 3, 2.1360990198241825);
//     tGate(q, 4);
//     sGate(q, 21);
//     pauliY(q, 10);
//     pauliY(q, 3);
//     controlledNot(q, 0, 3);
//     hadamard(q, 20);
//     rotateZ(q, 10, 2.0582553188574715);
//     controlledRotateX(q, 5, 19, 2.4998435452383356);
//     controlledRotateZ(q, 19, 25, 1.8933850820577571);
//     controlledPauliY(q, 28, 24);
//     pauliX(q, 4);
//     rotateY(q, 17, 3.6688156495083466);
//     rotateY(q, 19, 3.695812284364765);
//     controlledPauliY(q, 13, 9);
//     hadamard(q, 13);
//     pauliY(q, 11);
//     controlledRotateZ(q, 21, 9, 1.8183943613424502);
//     pauliZ(q, 2);
//     rotateY(q, 2, 4.380863475302177);
//     pauliX(q, 7);
//     controlledRotateX(q, 22, 5, 4.865214351250727);
//     pauliX(q, 19);
//     rotateZ(q, 28, 1.932253062148213);
//     controlledRotateZ(q, 23, 15, 3.112470740303239);
//     rotateZ(q, 16, 1.8761151754045318);
//     sGate(q, 22);
//     pauliY(q, 12);
//     rotateZ(q, 26, 6.090173710054208);
//     controlledNot(q, 20, 5);
//     rotateY(q, 11, 1.4595557147949088);
//     tGate(q, 1);
//     pauliY(q, 28);
//     controlledRotateX(q, 2, 27, 3.9784940979195618);
//     rotateZ(q, 17, 5.225103939672834);
//     rotateZ(q, 0, 4.344097059019724);
//     pauliX(q, 0);
//     controlledRotateY(q, 24, 19, 3.78173130652691);
//     pauliY(q, 2);
//     rotateX(q, 27, 5.249546596584958);
//     pauliX(q, 13);
//     controlledNot(q, 16, 24);
//     controlledRotateY(q, 17, 1, 1.5952186145317488);
//     hadamard(q, 16);
//     pauliY(q, 23);
//     rotateX(q, 5, 5.718019179976223);
//     pauliZ(q, 9);
//     controlledRotateY(q, 15, 6, 0.8426492713135303);
//     sGate(q, 6);
//     rotateZ(q, 25, 0.2444689509368444);
//     pauliY(q, 29);
//     pauliZ(q, 2);
//     pauliX(q, 2);
//     pauliZ(q, 25);
//     pauliZ(q, 2);
//     controlledRotateY(q, 18, 5, 0.11004043075099333);
//     pauliY(q, 15);
//     pauliY(q, 20);
//     rotateZ(q, 0, 1.6736066573980115);
//     rotateZ(q, 15, 5.742598516948371);
//     pauliZ(q, 24);
//     pauliX(q, 4);
//     controlledPauliY(q, 21, 6);
//     pauliY(q, 6);
//     pauliZ(q, 17);
//     sGate(q, 26);
//     pauliX(q, 29);
//     controlledRotateY(q, 4, 1, 3.5750143461044614);
//     controlledRotateY(q, 4, 1, 1.1896704014775608);
//     controlledRotateX(q, 23, 18, 3.2417273934494397);
//     pauliX(q, 21);
//     controlledRotateY(q, 10, 26, 0.05839076552808098);
//     controlledNot(q, 1, 29);
//     controlledPauliY(q, 5, 15);
//     pauliZ(q, 26);
//     controlledNot(q, 28, 0);
//     rotateX(q, 1, 4.608787191978134);
//     rotateX(q, 17, 4.810475273434427);
//     controlledNot(q, 24, 0);
//     tGate(q, 26);
//     controlledNot(q, 3, 25);
//     controlledRotateZ(q, 11, 2, 5.673742291053963);
//     rotateX(q, 8, 0.5980632244137037);
//     controlledPauliY(q, 6, 10);
//     controlledRotateY(q, 0, 5, 4.812045027373985);
//     controlledRotateX(q, 27, 2, 4.251963926456347);
//     controlledRotateX(q, 16, 23, 5.9995677915141306);
//     sGate(q, 3);
//     hadamard(q, 15);
//     controlledNot(q, 0, 20);
//     pauliY(q, 29);
//     pauliZ(q, 23);
//     rotateY(q, 27, 5.84907917718405);
//     hadamard(q, 11);
//     rotateX(q, 9, 2.495528546727144);
//     tGate(q, 4);
//     rotateX(q, 10, 1.7202188183243097);
//     pauliX(q, 10);
//     pauliX(q, 19);
//     pauliZ(q, 15);
//     rotateX(q, 6, 1.5132523517883625);
//     sGate(q, 5);
//     pauliY(q, 10);
//     controlledRotateY(q, 17, 21, 1.1222508351754412);
//     rotateX(q, 11, 1.625827218704169);
//     controlledRotateY(q, 26, 5, 1.1403011162743206);
//     pauliX(q, 24);
//     controlledRotateY(q, 14, 0, 6.098496028373251);
//     controlledNot(q, 23, 17);
//     pauliZ(q, 22);
//     rotateZ(q, 19, 2.916605695765121);
//     controlledNot(q, 26, 4);
//     controlledRotateZ(q, 14, 2, 2.0687881827873054);
//     pauliX(q, 15);
//     controlledRotateZ(q, 13, 9, 1.1791058019611314);
//     pauliZ(q, 21);
//     controlledRotateZ(q, 12, 9, 4.664652305517956);
//     controlledPauliY(q, 18, 12);
//     controlledRotateZ(q, 29, 2, 4.687640645073623);
//     tGate(q, 3);
//     pauliY(q, 7);
//     controlledRotateX(q, 28, 21, 3.098345732514666);
//     controlledRotateX(q, 0, 13, 2.4114954225822993);
//     pauliX(q, 20);
//     controlledRotateZ(q, 18, 6, 0.5910536662583553);
//     controlledRotateX(q, 22, 18, 5.218220544549179);
//     pauliX(q, 5);
//     hadamard(q, 13);
//     controlledRotateZ(q, 5, 1, 4.504664006376974);
//     controlledPauliY(q, 13, 18);
//     controlledPauliY(q, 15, 9);
//     tGate(q, 29);
//     rotateZ(q, 14, 6.152668283215903);
//     controlledRotateY(q, 23, 7, 5.506268676958865);
//     controlledRotateZ(q, 2, 28, 3.599985412498198);
//     rotateX(q, 12, 3.6345960506333306);
//     rotateZ(q, 24, 1.046023454469693);
//     controlledNot(q, 9, 20);
//     controlledRotateY(q, 23, 22, 1.3171681657364902);
//     rotateY(q, 22, 0.7954538044052196);
//     tGate(q, 10);
//     pauliY(q, 0);
//     rotateX(q, 23, 3.805042591052823);
//     controlledNot(q, 19, 0);
//     pauliY(q, 25);
//     rotateY(q, 0, 2.84819167577557);
//     controlledNot(q, 9, 8);
//     controlledNot(q, 12, 11);
//     pauliZ(q, 23);
//     controlledPauliY(q, 24, 4);
//     rotateX(q, 4, 4.7848845034283025);
//     pauliZ(q, 19);
//     rotateY(q, 7, 3.81228166969204);
//     pauliY(q, 27);
//     pauliX(q, 28);
//     pauliY(q, 11);
//     pauliZ(q, 4);
//     sGate(q, 12);
//     controlledRotateX(q, 9, 18, 0.16466184884428386);
//     controlledRotateY(q, 25, 18, 2.0166887901691313);
//     hadamard(q, 0);
//     controlledNot(q, 0, 25);
//     pauliZ(q, 6);
//     tGate(q, 7);
//     pauliX(q, 23);
//     controlledPauliY(q, 2, 9);
//     sGate(q, 24);
//     rotateZ(q, 12, 0.05369757127211415);
//     hadamard(q, 15);
//     controlledRotateX(q, 10, 21, 0.5954058723775156);
//     pauliX(q, 14);
//     controlledPauliY(q, 1, 18);
//     sGate(q, 20);
//     rotateY(q, 12, 2.604985117781478);
//     controlledNot(q, 9, 26);
//     rotateZ(q, 24, 1.8164555210254119);
//     controlledPauliY(q, 21, 12);
//     controlledNot(q, 19, 17);
//     rotateX(q, 25, 2.852692622378647);
//     sGate(q, 22);

//     printf("\n");
//     for(long long int i=0; i<30; ++i){
//         q_measure[i] = calcProbOfOutcome(q,  i, 1);
//         //printf("  probability for q[%2lld]==1 : %lf    \n", i, q_measure[i]);
//         fprintf(fp, "Probability for q[%2lld]==1 : %lf    \n", i, q_measure[i]);
//     }
//     fprintf(fp, "\n");
//     printf("\n");


//     for(int i=0; i<10; ++i){
//         Complex amp = getAmp(q, i);
//         //printf("Amplitude of %dth state vector: %f\n", i, prob);
// 	fprintf(fvec, "Amplitude of %dth state vector: %12.6f,%12.6f\n", i, amp.real, amp.imag);
//     }

//     double t2 = get_wall_time();
//     printf("Complete the simulation takes time %12.6f seconds.", t2 - t1);
//     printf("\n");
//     destroyQureg(q, Env);
//     destroyQuESTEnv(Env);

//     return 0;
// }
/** @file 
 * A demo of QuEST
 *
 * @author Ania Brown
 * @author Tyson Jones
 */

#include <stdio.h>
#include "QuEST.h"

int main (int narg, char *varg[]) {



    /*
     * PREPARE QuEST environment
     * (Required only once per program)
     */

    QuESTEnv env = createQuESTEnv();

    printf("-------------------------------------------------------\n");
    printf("Running QuEST tutorial:\n\t Basic circuit involving a system of 3 qubits.\n");
    printf("-------------------------------------------------------\n");



    /*
     * PREPARE QUBIT SYSTEM
     */

    Qureg qubits = createQureg(29, env);
    initZeroState(qubits);



    /*
     * REPORT SYSTEM AND ENVIRONMENT
     */
    printf("\nThis is our environment:\n");
    reportQuregParams(qubits);
    reportQuESTEnv(env);



    /*
     * APPLY CIRCUIT
     */

    hadamard(qubits, 0);
    controlledNot(qubits, 0, 1);
    rotateY(qubits, 2, .1);

    int targs[] = {0,1,2};
    multiControlledPhaseFlip(qubits, targs, 3);

    ComplexMatrix2 u = {
        .real={{.5,.5},{.5,.5}},
        .imag={{.5,-.5},{-.5,.5}}
    };
    unitary(qubits, 0, u);

    Complex a, b;
    a.real = .5; a.imag =  .5;
    b.real = .5; b.imag = -.5;
    compactUnitary(qubits, 1, a, b);

    Vector v;
    v.x = 1; v.y = 0; v.z = 0;
    rotateAroundAxis(qubits, 2, 3.14/2, v);

    controlledCompactUnitary(qubits, 0, 1, a, b);

    int ctrls[] = {0,1};
    multiControlledUnitary(qubits, ctrls, 2, 2, u);
    
    ComplexMatrixN toff = createComplexMatrixN(3);
    toff.real[6][7] = 1;
    toff.real[7][6] = 1;
    for (int i=0; i<6; i++)
        toff.real[i][i] = 1;
    multiQubitUnitary(qubits, targs, 3, toff);
    
    
    
    /*
     * STUDY QUANTUM STATE
     */

    printf("\nCircuit output:\n");

    qreal prob;
    prob = getProbAmp(qubits, 7);
    printf("Probability amplitude of |111>: %g\n", prob);

    prob = calcProbOfOutcome(qubits, 2, 1);
    printf("Probability of qubit 2 being in state 1: %g\n", prob);

    int outcome = measure(qubits, 0);
    printf("Qubit 0 was measured in state %d\n", outcome);

    outcome = measureWithStats(qubits, 2, &prob);
    printf("Qubit 2 collapsed to %d with probability %g\n", outcome, prob);



    /*
     * FREE MEMORY
     */
    while(1);
    destroyQureg(qubits, env); 
    destroyComplexMatrixN(toff);



    /*
     * CLOSE QUEST ENVIRONMET
     * (Required once at end of program)
     */
    destroyQuESTEnv(env);
    return 0;
}