//
//  RoundTripTests.m
//  HomeGrownTests
//
//  Created by Magneto on 4/6/26.
//

#import <Foundation/Foundation.h>
#import "RoundTripTests.h"
#include "TestBundle.hpp"
#include "TestBundleWithHooks.hpp"
#include "TestUnbundleWithHooks.hpp"
#include "WrappedArchiveAssembler.hpp"
#include "FakeMutation.hpp"
#include "TestUnbundle.hpp"
#include "TestRecover.hpp"

@implementation RoundTripTests


+ (BOOL) run_HappyFlow: (JobBundle &)pJob withArchives: (vector<FakeArchive> *)pArchives {
 
    /*
    if (pMockArchives == NULL) {
        printf("Error: mock archive vector missing...\n");
        return NO;
    }
    */
    
    
    MockHardDrive aHardDrive;
    MockFileSystem aFileSystem(&aHardDrive);
    ByteString aErrorString;
    
    if (!TestBundleWithHooks::PerformReal(pJob, aFileSystem, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    vector<WrappedArchive> aRealArchives = WrappedArchiveAssembler::Get(pJob.mArchived.ToString(),
                                                                        aFileSystem,
                                                                        pJob.mBlocksPerArchive,
                                                                        pJob.mPayloadBytesPerBlock + Layout::SectionHeaderSize());
    
    pArchives->clear();
    if (!TestBundle::PerformMock(pJob, pArchives, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    if (!BundleVerify::Execute(pJob, &aRealArchives, pArchives, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    if (!TestUnbundleWithHooks::PerformRealUnbundle(pJob, aFileSystem, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    vector <FakeFile> aFilesReal;
    if (!TestUnbundleWithHooks::CollectFiles(pJob, &aFilesReal, &aFileSystem, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    if (!UnbundleVerify::Execute(&aFilesReal, &pJob.mFileList, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    vector <FakeFile> aFilesMock;
    if (!TestUnbundle::PerformMock(pJob, pArchives, &aFilesMock, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    if (!UnbundleVerify::Execute(&aFilesReal, &aFilesMock, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    if (!UnbundleVerify::Execute(&pJob.mFileList, &aFilesMock, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        return NO;
    }
    
    return YES;
}

+ (BOOL) run_CorruptUnbundle: (JobBundle &)pJob withMutations: (vector<FakeMutation> *)pMutations {
    
    MockHardDrive aHardDrive;
    MockFileSystem aFileSystem(&aHardDrive);
    ByteString aErrorString;
    
    if (!TestBundleWithHooks::PerformReal(pJob, aFileSystem, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        Layout::PrintPackedMembership(pJob);
        return NO;
    }
    
    vector<FakeArchive> aMockArchives;
    if (!TestBundle::PerformMock(pJob, &aMockArchives, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        Layout::PrintPackedMembership(pJob);
        return NO;
    }
    
    if (!MutationTool::ApplyMutationsReal(pJob, pMutations, &aHardDrive, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        Layout::PrintPackedMembership(pJob);
        return NO;
    }
    
    // ApplyMutationsMock adjusts later mutation indices in place, so each round-trip
    // phase needs its own copy to avoid carrying shifted indices into the next phase.
    vector<FakeMutation> aMockMutations = *pMutations;
    if (!MutationTool::ApplyMutationsMock(&aMockMutations, &aMockArchives, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        Layout::PrintPackedMembership(pJob);
        return NO;
    }

    if (!TestUnbundleWithHooks::PerformRealUnbundle(pJob, aFileSystem, &aErrorString)) {
        
        
        
        // In this case, maybe we can recover 0 files.
        
        vector <FakeFile> aFilesMock;
        if (!TestUnbundle::PerformMock(pJob, &aMockArchives, &aFilesMock, &aErrorString)) {
            printf("ErrorA: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
            printf("ErrorB: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
            Layout::PrintPackedMembership(pJob);
            return NO;
        }
        
        vector <FakeFile> aEmptyFileList;
        if (!UnbundleVerify::Execute(&aEmptyFileList, &aFilesMock, &aErrorString)) {
            printf("ErrorA: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
            printf("ErrorB: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
            Layout::PrintPackedMembership(pJob);
            return NO;
        }
        
        if (aFilesMock.size() == 0) {
            return YES;
        } else {
            printf("Error: Mock files was not empty, real files was empty.\n");
            Layout::PrintPackedMembership(pJob);
            return NO;
        }
    }
    
    vector <FakeFile> aFilesReal;
    if (!TestUnbundleWithHooks::CollectFiles(pJob, &aFilesReal, &aFileSystem, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        Layout::PrintPackedMembership(pJob);
        return NO;
    }
    
    vector <FakeFile> aFilesMock;
    if (!TestUnbundle::PerformMock(pJob, &aMockArchives, &aFilesMock, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        Layout::PrintPackedMembership(pJob);
        return NO;
    }
    
    if (!UnbundleVerify::Execute(&aFilesReal, &aFilesMock, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        Layout::PrintPackedMembership(pJob);
        return NO;
    }
    
    return YES;
}

+ (BOOL) run_CorruptRecover: (JobBundle &)pJob withMutations: (vector<FakeMutation> *)pMutations {
    
    MockHardDrive aHardDrive;
    MockFileSystem aFileSystem(&aHardDrive);
    ByteString aErrorString;
    
    if (!TestBundleWithHooks::PerformReal(pJob, aFileSystem, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        Layout::PrintPackedMembership(pJob);
        return NO;
    }
    
    vector<FakeArchive> aMockArchives;
    if (!TestBundle::PerformMock(pJob, &aMockArchives, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        Layout::PrintPackedMembership(pJob);
        return NO;
    }
    
    
    if (!MutationTool::ApplyMutationsReal(pJob, pMutations, &aHardDrive, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        Layout::PrintPackedMembership(pJob);
        return NO;
    }
    
    // ApplyMutationsMock adjusts later mutation indices in place, so each round-trip
    // phase needs its own copy to avoid carrying shifted indices into the next phase.
    vector<FakeMutation> aMockMutations = *pMutations;
    if (!MutationTool::ApplyMutationsMock(&aMockMutations, &aMockArchives, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        Layout::PrintPackedMembership(pJob);
        return NO;
    }

    if (!TestUnbundleWithHooks::PerformRealRecover(pJob, aFileSystem, &aErrorString)) {
        
        
        
        // In this case, maybe we can recover 0 files.
        
        vector <FakeFile> aFilesMock;
        if (!TestRecover::PerformMock(pJob, &aMockArchives, &aFilesMock, &aErrorString)) {
            printf("ErrorA: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
            printf("ErrorB: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
            Layout::PrintPackedMembership(pJob);
            return NO;
        }
        
        vector <FakeFile> aEmptyFileList;
        if (!UnbundleVerify::Execute(&aEmptyFileList, &aFilesMock, &aErrorString)) {
            printf("ErrorA: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
            printf("ErrorB: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
            Layout::PrintPackedMembership(pJob);
            return NO;
        }
        
        if (aFilesMock.size() == 0) {
            return YES;
        } else {
            printf("Error: Mock files was not empty, real files was empty.\n");
            Layout::PrintPackedMembership(pJob);
            return NO;
        }
    }
    
    vector <FakeFile> aFilesReal;
    if (!TestUnbundleWithHooks::CollectFiles(pJob, &aFilesReal, &aFileSystem, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        Layout::PrintPackedMembership(pJob);
        return NO;
    }
    
    vector <FakeFile> aFilesMock;
    if (!TestRecover::PerformMock(pJob, &aMockArchives, &aFilesMock, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        Layout::PrintPackedMembership(pJob);
        return NO;
    }
    
    if (!UnbundleVerify::Execute(&aFilesReal, &aFilesMock, &aErrorString)) {
        printf("Error: %.*s\n", aErrorString.mLength, (char*)aErrorString.mData);
        Layout::PrintPackedMembership(pJob);
        return NO;
    }
    
    return YES;
}

@end
