//
//  WKWallet.c
//  WalletKitCore
//
//  Created by Ed Gamble on 3/19/19.
//  Copyright © 2019 Breadwinner AG. All rights reserved.
//
//  See the LICENSE file at the project root for license information.
//  See the CONTRIBUTORS file at the project root for a list of contributors.

#include "support/BROSCompat.h"
#include "support/BRFileService.h"
#include "support/BRCrypto.h"

#include "walletkit/WKSystemP.h"
#include "walletkit/WKNetworkP.h"
#include "walletkit/WKWalletManagerP.h"
#include "walletkit/WKClientP.h"
#include "walletkit/WKListenerP.h"

#include <stdio.h>                  // sprintf

// MARK: - All Systems

#if defined (NOT_WORKABLE_NEEDS_TO_REFERENCE_THE_SWIFT__JAVA_INSTANCE)
static pthread_once_t  _wkAllSystemsOnce   = PTHREAD_ONCE_INIT;
static pthread_mutex_t _wkAllSystemsMutex;

//
// We can't create a Swift/Java System based solely on WKSystem - because the System
// holds references to SystemClient and SystemListener which are not - and can never be - in the
// C code.
//
// We'd like to keep a table of all Systems herein; rather than distinctly in the Swift and the
// Java and the <other languages>.  To do so, requires a reference to the Swift/Java/other
// System.  Not sure what to hold, if anything.
//
// We keep System because WKListener, created in the Swift/Java, needs a 'Listener Context'
// which must be something that allows the Swift/Java to find the System.  
//
typedef struct {
    char *uids;
    WKSystem system;
    int foo;
    int bar;
} WKSystemEntry;

static BRArrayOf(WKSystemEntry) systems = NULL;

#define NUMBER_OF_SYSTEMS_DEFAULT       (3)

static void
_wkAllSystemsInitOnce  (void) {
    array_new (systems, NUMBER_OF_SYSTEMS_DEFAULT);
    pthread_mutex_init(&_wkAllSystemsMutex, NULL);
    // ...
}

static void
wkAllSystemsInit (void) {
    pthread_once (&_wkAllSystemsOnce, _wkAllSystemsInitOnce);
}

static WKSystemEntry *
_wkAllSystemsFind (WKSystem system) {
    wkAllSystemsInit();
    pthread_mutex_lock (&_wkAllSystemsMutex);
    WKSystemEntry *entry = NULL;
    pthread_mutex_unlock (&_wkAllSystemsMutex);

    return entry;
}

static WKSystemEntry *
_wkAllSystemsFindByUIDS (const char *uids) {
    wkAllSystemsInit();
    pthread_mutex_lock (&_wkAllSystemsMutex);
    WKSystemEntry *entry = NULL;
    pthread_mutex_unlock (&_wkAllSystemsMutex);

    return entry;
}

static void
wkAllSystemsAdd (WKSystem system) { // , void* context)
    wkAllSystemsInit();

    WKSystemEntry entry = {
        strdup (wkSystemGetResolvedPath(system)),
        wkSystemTake(system),
        0,
        0
    };
    pthread_mutex_lock (&_wkAllSystemsMutex);
    array_add (systems, entry);
    pthread_mutex_unlock (&_wkAllSystemsMutex);
}
#endif // defined (NOT_WORKABLE_NEEDS_TO_REFERENCE_THE_SWIFT__JAVA_INSTANCE)

// MARK: - System File Service

#define FILE_SERVICE_TYPE_CURRENCY_BUNDLE      "currency-bundle"

enum {
    FILE_SERVICE_TYPE_CURRENCY_BUNDLE_VERSION_1
};

static UInt256
fileServiceTypeCurrencyBundleV1Identifier (BRFileServiceContext context,
                                           BRFileService fs,
                                           const void *entity) {
    WKSystem system = (WKSystem) context; (void) system;
    const WKClientCurrencyBundle bundle = (const WKClientCurrencyBundle) entity;

    UInt256 identifier;

    BRSHA256 (identifier.u8, bundle->id, strlen(bundle->id));

    return identifier;
}

static void *
fileServiceTypeCurrencyBundleV1Reader (BRFileServiceContext context,
                                    BRFileService fs,
                                    uint8_t *bytes,
                                    uint32_t bytesCount) {
    WKSystem system = (WKSystem) context; (void) system;

    BRRlpCoder coder = rlpCoderCreate();
    BRRlpData  data  = (BRRlpData) { bytesCount, bytes };
    BRRlpItem  item  = rlpDataGetItem (coder, data);

    WKClientCurrencyBundle bundle = wkClientCurrencyBundleRlpDecode(item, coder);

    rlpItemRelease (coder, item);
    rlpCoderRelease(coder);

    return bundle;
}

static uint8_t *
fileServiceTypeCurrencyBundleV1Writer (BRFileServiceContext context,
                                       BRFileService fs,
                                       const void* entity,
                                       uint32_t *bytesCount) {
    WKSystem system = (WKSystem) context; (void) system;
    const WKClientCurrencyBundle bundle = (const WKClientCurrencyBundle) entity;

    BRRlpCoder coder = rlpCoderCreate();
    BRRlpItem  item  = wkClientCurrencyBundleRlpEncode (bundle, coder);
    BRRlpData  data  = rlpItemGetData (coder, item);

    rlpItemRelease  (coder, item);
    rlpCoderRelease (coder);

    *bytesCount = (uint32_t) data.bytesCount;
    return data.bytes;
}

static BRSetOf (WKClientCurrencyBundle)
wkSystemInitialCurrencyBundlesLoad (WKSystem system) {
    if (NULL == system || NULL == system->fileService) return NULL;

    BRSetOf(WKClientCurrencyBundle) bundles =  wkClientCurrencyBundleSetCreate(100);

    if (fileServiceHasType (system->fileService, FILE_SERVICE_TYPE_CURRENCY_BUNDLE) &&
        1 != fileServiceLoad (system->fileService, bundles, FILE_SERVICE_TYPE_CURRENCY_BUNDLE, 1)) {
        printf ("CRY: system failed to load currency bundles\n");
        wkClientCurrencyBundleSetRelease(bundles);
        return NULL;
    }

    return bundles;
}

static void
wkSystemFileServiceErrorHandler (BRFileServiceContext context,
                                            BRFileService fs,
                                            BRFileServiceError error) {
    switch (error.type) {
        case FILE_SERVICE_IMPL:
            // This actually a FATAL - an unresolvable coding error.
            printf ("CRY: System FileService Error: IMPL: %s\n", error.u.impl.reason);
            break;
        case FILE_SERVICE_UNIX:
            printf ("CRY: System FileService Error: UNIX: %s\n", strerror(error.u.unx.error));
            break;
        case FILE_SERVICE_ENTITY:
            // This is likely a coding error too.
            printf ("CRY: System FileService Error: ENTITY (%s): %s\n",
                         error.u.entity.type,
                         error.u.entity.reason);
            break;
        case FILE_SERVICE_SDB:
            printf ("CRY: System FileService Error: SDB: (%d): %s\n",
                         error.u.sdb.code,
                         error.u.sdb.reason);
            break;
    }
}

static BRFileServiceTypeSpecification systemFileServiceSpecifications[] = {
    FILE_SERVICE_TYPE_CURRENCY_BUNDLE,
    FILE_SERVICE_TYPE_CURRENCY_BUNDLE_VERSION_1,
    1,
    {
        {
            FILE_SERVICE_TYPE_CURRENCY_BUNDLE_VERSION_1,
            fileServiceTypeCurrencyBundleV1Identifier,
            fileServiceTypeCurrencyBundleV1Reader,
            fileServiceTypeCurrencyBundleV1Writer
        }
    }
};

static size_t systemFileServiceSpecificationsCount = (sizeof (systemFileServiceSpecifications) / sizeof (BRFileServiceTypeSpecification));

// MARK: - System

IMPLEMENT_WK_GIVE_TAKE (WKSystem, wkSystem)

extern WKSystem
wkSystemCreate (WKClient client,
                    WKListener listener,
                    WKAccount account,
                    const char *basePath,
                    WKBoolean onMainnet) {
    WKSystem system = calloc (1, sizeof (struct WKSystemRecord));

    system->state       = WK_SYSTEM_STATE_CREATED;
    system->onMainnet   = onMainnet;
    system->isReachable = WK_TRUE;
    system->client      = client;
    system->listener    = wkListenerTake (listener);
    system->account     = wkAccountTake  (account);

    // Build a `path` specific to `account`
    char *accountFileSystemIdentifier = wkAccountGetFileSystemIdentifier(account);
    system->path = malloc (strlen(basePath) + 1 + strlen(accountFileSystemIdentifier) + 1);
    sprintf (system->path, "%s/%s", basePath, accountFileSystemIdentifier);
    free (accountFileSystemIdentifier);

    // Create the system-state file service
    system->fileService = fileServiceCreateFromTypeSpecifications (system->path, "system", "state",
                                                                   system,
                                                                   wkSystemFileServiceErrorHandler,
                                                                   systemFileServiceSpecificationsCount,
                                                                   systemFileServiceSpecifications);

    // Fill in the builtin networks
    size_t networksCount = 0;
    WKNetwork *networks = wkNetworkInstallBuiltins (&networksCount,
                                                              wkListenerCreateNetworkListener(listener, system),
                                                              WK_TRUE == onMainnet);
    array_new (system->networks, networksCount);
    array_add_array (system->networks, networks, networksCount);
    free (networks);

    // Start w/ no `managers`; never more than `networksCount`
    array_new (system->managers, networksCount);

    system->ref = WK_REF_ASSIGN (wkSystemRelease);

    pthread_mutex_init_brd (&system->lock, PTHREAD_MUTEX_NORMAL);  // PTHREAD_MUTEX_RECURSIVE

    // Extract currency bundles
    BRSetOf (WKClientCurrencyBundle) currencyBundles = wkSystemInitialCurrencyBundlesLoad (system);
    if (NULL != currencyBundles) {
        FOR_SET (WKClientCurrencyBundle, currencyBundle, currencyBundles) {
            WKNetwork network = wkSystemGetNetworkForUids (system, currencyBundle->bid);
            if (NULL != network) {
                wkNetworkAddCurrencyAssociationFromBundle (network, currencyBundle, WK_FALSE);
            }
        }
        wkClientCurrencyBundleSetRelease(currencyBundles);
    }

    // The System has been created.
    wkSystemGenerateEvent (system, (WKSystemEvent) {
        WK_SYSTEM_EVENT_CREATED
    });

    // Each Network has been added to System.
    for (size_t index = 0; index < array_count(system->networks); index++)
        wkSystemGenerateEvent (system, (WKSystemEvent) {
            WK_SYSTEM_EVENT_NETWORK_ADDED,
            { .network = wkNetworkTake (system->networks[index]) }
        });

    // All the available networks have been discovered
    wkSystemGenerateEvent (system, (WKSystemEvent) {
        WK_SYSTEM_EVENT_DISCOVERED_NETWORKS
    });

#if defined (NOT_WORKABLE_NEEDS_RO_REFERENCE_THE_SWIFT__JAVA_INSTANCE)
    wkAllSystemsAdd (system);
#endif

    return system;
}

static void
wkSystemRelease (WKSystem system) {
    pthread_mutex_lock (&system->lock);
    wkSystemSetState (system, WK_SYSTEM_STATE_DELETED);

    // Networks
    array_free_all (system->networks, wkNetworkGive);

    // Managers
    array_free_all (system->managers, wkWalletManagerGive);

    wkAccountGive  (system->account);
    wkListenerGive (system->listener);
    free (system->path);

    pthread_mutex_unlock  (&system->lock);
    pthread_mutex_destroy (&system->lock);

    #if 0
        wkWalletGenerateEvent (wallet, (WKWalletEvent) {
            WK_WALLET_EVENT_DELETED
        });
    #endif

    memset (system, 0, sizeof(*system));
    free (system);
}

extern WKBoolean
wkSystemOnMainnet (WKSystem system) {
    return system->onMainnet;
}

extern WKBoolean
wkSystemIsReachable (WKSystem system) {
    pthread_mutex_lock (&system->lock);
    WKBoolean result = system->isReachable;
    pthread_mutex_unlock (&system->lock);

    return result;
}

extern void
wkSystemSetReachable (WKSystem system,
                          WKBoolean isReachable) {
    pthread_mutex_lock (&system->lock);
    system->isReachable = isReachable;
    for (size_t index = 0; index < array_count(system->managers); index++)
        wkWalletManagerSetNetworkReachable (system->managers[index], isReachable);
    pthread_mutex_unlock (&system->lock);
}

extern const char *
wkSystemGetResolvedPath (WKSystem system) {
    return system->path;
}

extern WKSystemState
wkSystemGetState (WKSystem system) {
    pthread_mutex_lock (&system->lock);
    WKSystemState state = system->state;
    pthread_mutex_unlock (&system->lock);

    return state;
}

private_extern void
wkSystemSetState (WKSystem system,
                      WKSystemState state) {
    pthread_mutex_lock (&system->lock);

    WKSystemState newState = state;
    WKSystemState oldState = system->state;

    system->state = state;

    if (oldState != newState)
         wkSystemGenerateEvent (system, (WKSystemEvent) {
             WK_SYSTEM_EVENT_CHANGED,
             { .state = { oldState, newState }}
         });
    pthread_mutex_unlock (&system->lock);
}

extern const char *
wkSystemEventTypeString (WKSystemEventType type) {
    static const char *names[] = {
        "WK_WALLET_EVENT_CREATED",
        "WK_SYSTEM_EVENT_CHANGED",
        "WK_SYSTEM_EVENT_DELETED",

        "WK_SYSTEM_EVENT_NETWORK_ADDED",
        "WK_SYSTEM_EVENT_NETWORK_CHANGED",
        "WK_SYSTEM_EVENT_NETWORK_DELETED",

        "WK_SYSTEM_EVENT_MANAGER_ADDED",
        "WK_SYSTEM_EVENT_MANAGER_CHANGED",
        "WK_SYSTEM_EVENT_MANAGER_DELETED",

        "WK_SYSTEM_EVENT_DISCOVERED_NETWORKS",
    };
    return names [type];
}

// MARK: - System Network

static WKBoolean
wkSystemHasNetworkFor (WKSystem system,
                       WKNetwork network,
                       size_t *forIndex,
                       bool needLock) {
    WKBoolean result = WK_FALSE;

    if (needLock) pthread_mutex_lock (&system->lock);
    for (size_t index = 0; index < array_count(system->networks); index++) {
        if (network == system->networks[index]) {
            if (forIndex) *forIndex = index;
            result = WK_TRUE;
            break;
        }
    }
    if (needLock) pthread_mutex_unlock (&system->lock);

    return result;
}

extern WKBoolean
wkSystemHasNetwork (WKSystem system,
                        WKNetwork network) {
    return wkSystemHasNetworkFor (system, network, NULL, true);
}

extern WKNetwork *
wkSystemGetNetworks (WKSystem system,
                         size_t *count) {
    WKNetwork *networks = NULL;

    pthread_mutex_lock (&system->lock);
    *count = array_count (system->networks);
    if (0 != *count) {
        networks = calloc (*count, sizeof(WKNetwork));
        for (size_t index = 0; index < *count; index++) {
            networks[index] = wkNetworkTake(system->networks[index]);
        }
    }
    pthread_mutex_unlock (&system->lock);

    return networks;
}

extern WKNetwork
 wkSystemGetNetworkAt (WKSystem system,
                           size_t index) {
     WKNetwork network = NULL;

     pthread_mutex_lock (&system->lock);
     if (index < array_count(system->networks))
         network = wkNetworkTake (system->networks[index]);
     pthread_mutex_unlock (&system->lock);

     return network;
}

static WKNetwork
wkSystemGetNetworkForUidsWithIndexNeedLock (WKSystem system,
                                            const char *uids,
                                            size_t *indexOfNetwork,
                                            bool needLock) {
    WKNetwork network = NULL;

    if (needLock) pthread_mutex_lock (&system->lock);
    for (size_t index = 0; index < array_count(system->networks); index++) {
        if (0 == strcmp (uids, wkNetworkGetUids(system->networks[index]))) {
            if (NULL != indexOfNetwork) *indexOfNetwork = index;
            network = wkNetworkTake (system->networks[index]);
            break;
        }
    }
    if (needLock) pthread_mutex_unlock (&system->lock);

    return network;
}

static WKNetwork
wkSystemGetNetworkForUidsWithIndex (WKSystem system,
                                    const char *uids,
                                    size_t *indexOfNetwork) {
    return wkSystemGetNetworkForUidsWithIndexNeedLock (system, uids, indexOfNetwork, true);
}

extern WKNetwork
wkSystemGetNetworkForUids (WKSystem system,
                               const char *uids) {
    return wkSystemGetNetworkForUidsWithIndex (system, uids, NULL);
}


extern size_t
wkSystemGetNetworksCount (WKSystem system) {
    pthread_mutex_lock (&system->lock);
    size_t count = array_count(system->networks);
    pthread_mutex_unlock (&system->lock);

    return count;
}

private_extern void
wkSystemAddNetwork (WKSystem system,
                        WKNetwork network) {
    pthread_mutex_lock (&system->lock);
    if (WK_FALSE == wkSystemHasNetworkFor (system, network, NULL, false)) {
        array_add (system->networks, wkNetworkTake(network));
        wkListenerGenerateSystemEvent (system->listener, system, (WKSystemEvent) {
            WK_SYSTEM_EVENT_NETWORK_ADDED,
            { .network = wkNetworkTake (network) }
        });
    }
    pthread_mutex_unlock (&system->lock);
}

private_extern void
wkSystemRemNetwork (WKSystem system,
                        WKNetwork network) {
    size_t index;
    pthread_mutex_lock (&system->lock);
    if (WK_TRUE == wkSystemHasNetworkFor (system, network, &index, false)) {
        array_rm (system->networks, index);
        wkListenerGenerateSystemEvent (system->listener, system, (WKSystemEvent) {
             WK_SYSTEM_EVENT_NETWORK_DELETED,
            { .network = network }  // no wkNetworkTake -> releases system->networks reference
         });
    }
    pthread_mutex_unlock (&system->lock);
}

private_extern void
wkSystemStartReceiveAddressSync (WKSystem system,
                              WKWalletManager manager) {
    wkListenerGenerateSystemEvent (system->listener, system, (WKSystemEvent) {
        WK_SYSTEM_EVENT_MANAGER_ADDED,
        { .manager = wkWalletManagerTake (manager) }
    });
}

// MARK: - System Wallet Managers

static WKBoolean
wkSystemHasWalletManagerFor (WKSystem system,
                             WKWalletManager manager,
                             size_t *forIndex,
                             bool needLock) {
    WKBoolean result = WK_FALSE;

    if (needLock) pthread_mutex_lock (&system->lock);
    for (size_t index = 0; index < array_count(system->managers); index++) {
        if (manager == system->managers[index]) {
            if (forIndex) *forIndex = index;
            result = WK_TRUE;
            break;
        }
    }
    if (needLock) pthread_mutex_unlock (&system->lock);

    return result;
}

extern WKBoolean
wkSystemHasWalletManager (WKSystem system,
                              WKWalletManager manager) {
    return wkSystemHasWalletManagerFor (system, manager, NULL, true);

}

extern WKWalletManager *
wkSystemGetWalletManagers (WKSystem system,
                               size_t *count) {
    WKWalletManager *managers = NULL;

    pthread_mutex_lock (&system->lock);
    *count = array_count (system->managers);
    if (0 != *count) {
        managers = calloc (*count, sizeof(WKWalletManager));
        for (size_t index = 0; index < *count; index++) {
            managers[index] = wkWalletManagerTake(system->managers[index]);
        }
    }
    pthread_mutex_unlock (&system->lock);

    return managers;
}

extern WKWalletManager
wkSystemGetWalletManagerAt (WKSystem system,
                                size_t index) {
    WKWalletManager manager = NULL;

    pthread_mutex_lock (&system->lock);
    if (index < array_count(system->managers))
        manager = wkWalletManagerTake (system->managers[index]);
    pthread_mutex_unlock (&system->lock);

    return manager;
}

extern WKWalletManager
wkSystemGetWalletManagerByNetwork (WKSystem system,
                                       WKNetwork network) {
    WKWalletManager manager = NULL;

    pthread_mutex_lock (&system->lock);
    for (size_t index = 0; index < array_count(system->managers); index++)
        if (wkWalletManagerHasNetwork (system->managers[index], network)) {
            manager = wkWalletManagerTake (system->managers[index]);
            break;
        }
    pthread_mutex_unlock (&system->lock);

    return manager;
}

static WKWalletManager
wkSystemGetWalletManagerByNetworkAndAccount (WKSystem system,
                                             WKNetwork network,
                                             WKAccount account) {
    WKWalletManager manager = wkSystemGetWalletManagerByNetwork (system, network);
    
    if (NULL != manager) {
        if (WK_FALSE == wkWalletManagerHasAccount(manager, account)) {
            wkWalletManagerGive(manager);
            manager = NULL;
        }
    }
    
    return manager;
}

extern size_t
wkSystemGetWalletManagersCount (WKSystem system) {
    pthread_mutex_lock (&system->lock);
    size_t count = array_count(system->managers);
    pthread_mutex_unlock (&system->lock);

    return count;
}

private_extern void
wkSystemAddWalletManager (WKSystem system,
                              WKWalletManager manager) {
    pthread_mutex_lock (&system->lock);
    if (WK_FALSE == wkSystemHasWalletManagerFor (system, manager, NULL, false)) {
        array_add (system->managers, wkWalletManagerTake(manager));
        wkListenerGenerateSystemEvent (system->listener, system, (WKSystemEvent) {
            WK_SYSTEM_EVENT_MANAGER_ADDED,
            { .manager = wkWalletManagerTake (manager) }
        });
    }
    pthread_mutex_unlock (&system->lock);
}

private_extern void
wkSystemRemWalletManager (WKSystem system,
                              WKWalletManager manager) {
    size_t index;

    pthread_mutex_lock (&system->lock);
    if (WK_TRUE == wkSystemHasWalletManagerFor (system, manager, &index, false)) {
        array_rm (system->managers, index);
        wkListenerGenerateSystemEvent (system->listener, system, (WKSystemEvent) {
             WK_SYSTEM_EVENT_MANAGER_DELETED,
            { .manager = manager }  // no wkNetworkTake -> releases system->managers reference
         });
    }
    pthread_mutex_unlock (&system->lock);
}

extern WKWalletManager
wkSystemCreateWalletManager (WKSystem system,
                             WKNetwork network,
                             WKSyncMode mode,
                             WKAddressScheme scheme,
                             WKCurrency *currencies,
                             size_t currenciesCount) {
    if (WK_FALSE == wkNetworkIsAccountInitialized (network, system->account)) {
        return NULL;
    }

    // Look for a pre-existing wallet manager
    WKWalletManager manager = wkSystemGetWalletManagerByNetworkAndAccount (system, network, system->account);

    if (NULL == manager) {

        manager = wkWalletManagerCreate (wkListenerCreateWalletManagerListener (system->listener, system),
                                         system->client,
                                         system->account,
                                         network,
                                         mode,
                                         scheme,
                                         system->path);

        wkSystemAddWalletManager (system, manager);

        wkWalletManagerSetNetworkReachable (manager, system->isReachable);

        for (size_t index = 0; index < currenciesCount; index++)
            if (wkNetworkHasCurrency (network, currencies[index]))
                wkWalletManagerCreateWallet (manager, currencies[index]);
    }

    // Start the event handler.
    wkWalletManagerStart (manager);

    return manager;
}

extern void wkSystemReceiveAddressSync (WKSystem system,
                                    WKWalletManager manager) {
    wkSystemStartReceiveAddressSync (system,
                            manager);
}

// MARK: - Currency

private_extern void
wkSystemHandleCurrencyBundles (WKSystem system,
                               OwnershipKept BRArrayOf (WKClientCurrencyBundle) bundles) {
    // Save the bundles straight away
    for (size_t bundleIndex = 0; bundleIndex < array_count(bundles); bundleIndex++)
        fileServiceSave (system->fileService, FILE_SERVICE_TYPE_CURRENCY_BUNDLE, bundles[bundleIndex]);

    pthread_mutex_lock (&system->lock);

    // Partition `bundles` by `network`

    size_t networksCount = array_count(system->networks);
    BRArrayOf (WKClientCurrencyBundle) bundlesForNetworks[networksCount];

    for (size_t index = 0; index < networksCount; index++)
        array_new (bundlesForNetworks[index], 10);

    size_t networkIndex = 0;
    for (size_t bundleIndex = 0; bundleIndex < array_count(bundles); bundleIndex++) {
        WKNetwork network = wkSystemGetNetworkForUidsWithIndexNeedLock (system, bundles[bundleIndex]->bid, &networkIndex, false);
        if (NULL != network)
            array_add (bundlesForNetworks[networkIndex], bundles[bundleIndex]);
    }

    // Add bundles applicable to each network

    for (size_t index = 0; index < networksCount; index++) {
        wkNetworkAddCurrencyAssociationsFromBundles (system->networks[index], bundlesForNetworks[index]);
        array_free (bundlesForNetworks[index]);
    }

    pthread_mutex_unlock (&system->lock);
}

// MARK: - System Control

extern void
wkSystemStart (WKSystem system) {
    wkListenerStart(system->listener);
    // client
    // query
}

extern void
wkSystemStop (WKSystem system) {
    // query
    // client
    wkListenerStop(system->listener);
}

extern void
wkSystemConnect (WKSystem system) {
    pthread_mutex_lock (&system->lock);
    for (size_t index = 0; index < array_count(system->managers); index++)
        wkWalletManagerConnect (system->managers[index], NULL);
    pthread_mutex_unlock (&system->lock);
}

extern void
wkSystemDisconnect (WKSystem system) {
    pthread_mutex_lock (&system->lock);
    for (size_t index = 0; index < array_count(system->managers); index++)
         wkWalletManagerDisconnect (system->managers[index]);
    pthread_mutex_unlock (&system->lock);
}
