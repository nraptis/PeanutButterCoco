//
//  Random.cpp
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 4/3/26.
//

#include "Random.hpp"

std::random_device cRandomDevice;
std::mt19937 cRandomGenerator(cRandomDevice());

void Random::Seed(int pSeed) {
    cRandomGenerator.seed(pSeed);
}

int Random::Get(int pMax) {
    if (pMax <= 0) {
        return 0;
    }
    std::uniform_int_distribution<int> aDist(0, pMax - 1);
    return aDist(cRandomGenerator);
}

int Random::Get(int pMin, int pMax) {
    if (pMin >= pMax) {
        return pMin;
    }
    std::uniform_int_distribution<int> aDist(pMin, pMax);
    return aDist(cRandomGenerator);
}

float Random::GetFloat() {
    std::uniform_real_distribution<float> aDist(0.0f, 1.0f);
    return aDist(cRandomGenerator);
}

float Random::GetFloat(float pMax) {
    if (pMax <= 0.0f) {
        return 0.0f;
    }
    std::uniform_real_distribution<float> aDist(0.0f, pMax);
    return aDist(cRandomGenerator);
}

float Random::GetFloat(float pMin, float pMax) {
    if (pMin >= pMax) {
        return pMin;
    }
    std::uniform_real_distribution<float> aDist(pMin, pMax);
    return aDist(cRandomGenerator);
}
