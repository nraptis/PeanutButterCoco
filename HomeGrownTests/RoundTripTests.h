//
//  RoundTripTests.m
//  HomeGrownTests
//
//  Created by Lucky Squirrel on 4/6/26.
//

#import <Foundation/Foundation.h>

#include "FakeFile.hpp"
#include "JobBundle.hpp"
#include "BundleVerify.hpp"
#include "UnbundleVerify.hpp"
#include "MutationTool.hpp"
#include "FakeMutation.hpp"

using namespace std;

@interface RoundTripTests: NSObject

//+ (BOOL) run_HappyFlow: (JobBundle &)pJob withFakeArchives: (vector<FakeArchive> *)pMockArchives;
+ (BOOL) run_HappyFlow: (JobBundle &)pJob withArchives: (vector<FakeArchive> *)pArchives;

+ (BOOL) run_CorruptUnbundle: (JobBundle &)pJob withMutations: (vector<FakeMutation> *)pMutations;


+ (BOOL) run_CorruptRecover: (JobBundle &)pJob withMutations: (vector<FakeMutation> *)pMutations;


// #include "FakeMutation.hpp"


//vector<FakeArchive> aMockArchives;

@end
