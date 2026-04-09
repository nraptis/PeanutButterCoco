//
//  Random.hpp
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 4/3/26.
//

#ifndef Random_hpp
#define Random_hpp

#include <random>
#include "namespaces.hpp"

class Random {
public:
    
    static void                         Seed(int pSeed);
    static int                          Get(int pMax);
    static int                          Get(int pMin, int pMax);
    static float                        GetFloat();
    static float                        GetFloat(float pMax);
    
    static float                        GetFloat(float pMin, float pMax);    
};

#endif /* Random_hpp */
