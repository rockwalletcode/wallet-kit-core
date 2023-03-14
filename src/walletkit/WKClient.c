//
//  WKWalletManagerClient.c
//  WK
//
//  Created by Michael Carrara on 6/19/19.
//  Copyright © 2019 Breadwinner AG. All rights reserved.
//
//  See the LICENSE file at the project root for license information.
//  See the CONTRIBUTORS file at the project root for a list of contributors.

#include "WKClientP.h"

#include <errno.h>
#include <math.h>  // round()
#include <stdbool.h>
#include <stdio.h>          // printf

#include "support/BRArray.h"
#include "support/BRCrypto.h"
#include "support/BROSCompat.h"

#include "WKAddressP.h"
#include "WKHashP.h"
#include "WKFileService.h"
#include "WKTransferP.h"
#include "WKNetworkP.h"
#include "WKWalletP.h"
#include "WKWalletManagerP.h"
#include "WKSystemP.h"

#define OFFSET_BLOCKS_IN_SECONDS       (3 * 24 * 60 * 60)  // 3 days

// MARK: - Error

extern const char *
wkClientErrorTypeDescription (WKClientErrorType type) {
    static const char *descriptions[] = {
        "Bad Reqeust",
        "Permission",
        "Resource",
        "Bad Response",
        "Submission",
        "Unavailable",
        "Lost Connectivity"
    };
    return descriptions[type];
}

static OwnershipGiven WKClientError
wkClientErrorCreateInternal (WKClientErrorType type, const char *details) {
    WKClientError error = calloc (1, sizeof (struct WKClientErrorRecord));

    error->type = type;
    error->details = (NULL == details ? NULL : strdup (details));

    return error;
}

extern OwnershipGiven WKClientError
wkClientErrorCreate (WKClientErrorType type, const char *details) {
    return wkClientErrorCreateInternal(type, details);
}

extern OwnershipGiven WKClientError
wkClientErrorCreateSubmission (WKTransferSubmitErrorType submitErrorType, const char *details) {
    WKClientError error = wkClientErrorCreateInternal (WK_CLIENT_ERROR_SUBMISSION, details);

    error->u.submitErrorType = submitErrorType;

    return error;
}

private_extern void
wkClientErrorRelease (WKClientError error) {
    if (NULL == error) return;
    
    if (NULL != error->details) free (error->details);

    memset (error, 0, sizeof (struct WKClientErrorRecord));
    free (error);
}

extern WKClientErrorType
wkClientErrorGetType (WKClientError error) {
    return error->type;
}

// MARK: Client Sync/Send Forward Declarations

static void
wkClientP2PManagerSync (WKClientP2PManager p2p, WKSyncDepth depth, WKBlockNumber height);

static void
wkClientP2PManagerSend (WKClientP2PManager p2p, WKWallet wallet, WKTransfer transfer);

static void
wkClientQRYManagerSync (WKClientQRYManager qry, WKSyncDepth depth, WKBlockNumber height);

static void
wkClientQRYManagerSend (WKClientQRYManager qry,  WKWallet wallet, WKTransfer transfer);

// MARK: Client Sync

extern void
wkClientSync (WKClientSync sync,
                  WKSyncDepth depth,
                  WKBlockNumber height) {
    switch (sync.type) {
        case WK_CLIENT_P2P_MANAGER_TYPE:
            wkClientP2PManagerSync (sync.u.p2pManager, depth, height);
            break;
        case WK_CLIENT_QRY_MANAGER_TYPE:
            wkClientQRYManagerSync (sync.u.qryManager, depth, height);
            break;
    }
}

extern void
wkClientSyncPeriodic (WKClientSync sync) {
    switch (sync.type) {
        case WK_CLIENT_P2P_MANAGER_TYPE:
            /* Nothing */
            break;
        case WK_CLIENT_QRY_MANAGER_TYPE:
            wkClientQRYManagerTickTock (sync.u.qryManager);
            break;
    }
}

// MARK: Client Send

extern void
wkClientSend (WKClientSend send,
                  WKWallet wallet,
                  WKTransfer transfer) {
    switch (send.type) {
        case WK_CLIENT_P2P_MANAGER_TYPE:
            wkClientP2PManagerSend (send.u.p2pManager, wallet, transfer);
            break;
        case WK_CLIENT_QRY_MANAGER_TYPE:
            wkClientQRYManagerSend (send.u.qryManager, wallet, transfer);
            break;
    }
}

// MARK: Client P2P (Peer-to-Peer)

extern WKClientP2PManager
wkClientP2PManagerCreate (size_t sizeInBytes,
                              WKNetworkType type,
                              const WKClientP2PHandlers *handlers) {
    assert (sizeInBytes >= sizeof (struct WKClientP2PManagerRecord));

    WKClientP2PManager p2pManager = calloc (1, sizeInBytes);

    p2pManager->type        = type;
    p2pManager->handlers    = handlers;
    p2pManager->sizeInBytes = sizeInBytes;

    pthread_mutex_init_brd (&p2pManager->lock, PTHREAD_MUTEX_NORMAL);

    return p2pManager;
}

extern void
wkClientP2PManagerRelease (WKClientP2PManager p2p) {
    p2p->handlers->release (p2p);
    memset (p2p, 0, p2p->sizeInBytes);
    free (p2p);
}

extern void
wkClientP2PManagerConnect (WKClientP2PManager p2p,
                               WKPeer peer) {
    p2p->handlers->connect (p2p, peer);
}

extern void
wkClientP2PManagerDisconnect (WKClientP2PManager p2p) {
    p2p->handlers->disconnect (p2p);
}

static void
wkClientP2PManagerSync (WKClientP2PManager p2p,
                            WKSyncDepth depth,
                            WKBlockNumber height) {
    p2p->handlers->sync (p2p, depth, height);
}

static void
wkClientP2PManagerSend (WKClientP2PManager p2p,
                            WKWallet   wallet,
                            WKTransfer transfer) {
    p2p->handlers->send (p2p, wallet, transfer);
}

extern void
wkClientP2PManagerSetNetworkReachable (WKClientP2PManager p2p,
                                           WKBoolean isNetworkReachable) {
    if (NULL != p2p->handlers->setNetworkReachable) {
        p2p->handlers->setNetworkReachable (p2p, WK_TRUE == isNetworkReachable);
    }
}

// MARK: Client QRY (QueRY)

static void wkClientQRYRequestBlockNumber  (WKClientQRYManager qry);
static void wkClientQRYRequestBlockNumberReceiveAddressSync  (WKClientQRYManager qry);
static bool wkClientQRYRequestTransactionsOrTransfers (WKClientQRYManager qry,
                                                           WKClientCallbackType type,
                                                           OwnershipKept  BRSetOf(WKAddress) oldAddresses,
                                                           OwnershipGiven BRSetOf(WKAddress) newAddresses,
                                                           size_t requestId);
static void wkClientQRYSubmitTransfer      (WKClientQRYManager qry,
                                                WKWallet   wallet,
                                                WKTransfer transfer);

extern WKClientQRYManager
wkClientQRYManagerCreate (WKClient client,
                              WKWalletManager manager,
                              WKClientQRYByType byType,
                              WKBlockNumber earliestBlockNumber,
                              WKBlockNumber currentBlockNumber) {
    WKClientQRYManager qry = calloc (1, sizeof (struct WKClientQRYManagerRecord));

    qry->client    = client;
    qry->manager   = manager;
    qry->requestId = 0;
    qry->byType    = byType;

    // For 'GET /transaction' we'll back up from the begBlockNumber by this offset.  Currently
    // about three days.  If the User has their App open continuously and if `GET /transactions`
    // fails for 2 days, then once it recovers, the App will get the 'missed' transactions back
    // from 3 days agao.
    qry->blockNumberOffset = OFFSET_BLOCKS_IN_SECONDS / manager->network->confirmationPeriodInSeconds;
    qry->blockNumberOffset = MAX (qry->blockNumberOffset, 100);

    // Initialize the `brdSync` struct
    qry->sync.rid = SIZE_MAX;
    qry->sync.begBlockNumber = earliestBlockNumber;
    qry->sync.endBlockNumber = MAX (earliestBlockNumber, currentBlockNumber);
    qry->sync.completed = true;
    qry->sync.success   = false;
    qry->sync.unbounded = WK_CLIENT_QRY_IS_UNBOUNDED;

    qry->connected = false;

    pthread_mutex_init_brd (&qry->lock, PTHREAD_MUTEX_NORMAL);
    return qry;
}

extern void
wkClientQRYManagerRelease (WKClientQRYManager qry) {
    // Try to ensure that `qry->lock` is unlocked prior to destroy.
    pthread_mutex_lock (&qry->lock);
    pthread_mutex_unlock (&qry->lock);
    // Tiny race
    pthread_mutex_destroy (&qry->lock);

    memset (qry, 0, sizeof(*qry));
    free (qry);
}

extern void
wkClientQRYManagerConnect (WKClientQRYManager qry) {
    pthread_mutex_lock (&qry->lock);
    qry->connected = true;
    wkWalletManagerSetState (qry->manager, wkWalletManagerStateInit (WK_WALLET_MANAGER_STATE_SYNCING));
    pthread_mutex_unlock (&qry->lock);

    // Start a sync immediately.
    wkClientQRYManagerTickTock (qry);
}

extern void
wkClientQRYManagerReceiveAddressSync (WKClientQRYManager qry) { // ADDED
//    pthread_mutex_lock (&qry->lock);
//    qry->connected = true;
//    wkWalletManagerSetState (qry->manager, wkWalletManagerStateInit (WK_WALLET_MANAGER_STATE_SYNCING));
//    pthread_mutex_unlock (&qry->lock);

    // Start a sync immediately.
    wkClientQRYManagerTickTockReceiveAddressSync (qry);
}

extern void
wkClientQRYManagerDisconnect (WKClientQRYManager qry) {
    pthread_mutex_lock (&qry->lock);
    wkWalletManagerSetState (qry->manager, wkWalletManagerStateInit (WK_WALLET_MANAGER_STATE_CONNECTED));
    qry->connected = false;
    pthread_mutex_unlock (&qry->lock);
}


static void
wkClientQRYManagerSync (WKClientQRYManager qry,
                            WKSyncDepth depth,
                            WKBlockNumber height) {

}

static WKBlockNumber
wkClientQRYGetNetworkBlockHeight (WKClientQRYManager qry){
    return wkNetworkGetHeight (qry->manager->network);
}

static void
wkClientQRYManagerSend (WKClientQRYManager qry,
                            WKWallet wallet,
                            WKTransfer transfer) {
    wkClientQRYSubmitTransfer (qry, wallet, transfer);
}

static void
wkClientQRYManagerUpdateSync (WKClientQRYManager qry,
                                  bool completed,
                                  bool success,
                                  bool needLock) {
    if (needLock) pthread_mutex_lock(&qry->lock);

    bool needBegEvent = !completed && qry->sync.completed;
    bool needEndEvent = completed && !qry->sync.completed;

    //
    // If the sync is an incremental sync - because the `begBlockNumber` is near to the current
    // network's block height, then we don't want events generated.  This makes the QRY events
    // similar to P2P events where as each block is announced NO sync event are generated.
    //
    // We are a little bit sloppy with avoiding the events.  Normally the `begBlockNumber` is
    // `blockNumberOffset` less than the current network's height - but we'll avoid events if
    // `begBlockNumber` is within `2 * blockNumberOffset`
    //
    if (qry->sync.begBlockNumber >= wkClientQRYGetNetworkBlockHeight (qry) - 2 * qry->blockNumberOffset) {
        needBegEvent = false;
        needEndEvent = false;
    }
    
    qry->sync.completed = completed;
    qry->sync.success   = success;

    if (needBegEvent) {
        wkWalletManagerSetState (qry->manager, (WKWalletManagerState) {
            WK_WALLET_MANAGER_STATE_SYNCING
        });

        wkWalletManagerGenerateEvent (qry->manager, (WKWalletManagerEvent) {
            WK_WALLET_MANAGER_EVENT_SYNC_STARTED
        });

        wkWalletManagerGenerateEvent (qry->manager, (WKWalletManagerEvent) {
            WK_WALLET_MANAGER_EVENT_SYNC_CONTINUES,
            { .syncContinues = { NO_WK_TIMESTAMP, 0 }}
        });
    }

    if (needEndEvent) {
        wkWalletManagerGenerateEvent (qry->manager, (WKWalletManagerEvent) {
            WK_WALLET_MANAGER_EVENT_SYNC_CONTINUES,
            { .syncContinues = { NO_WK_TIMESTAMP, 100 }}
        });

        wkWalletManagerGenerateEvent (qry->manager, (WKWalletManagerEvent) {
            WK_WALLET_MANAGER_EVENT_SYNC_STOPPED,
            { .syncStopped = (success
                              ? wkSyncStoppedReasonComplete()
                              : wkSyncStoppedReasonUnknown()) }
        });

        wkWalletManagerSetState (qry->manager, (WKWalletManagerState) {
            WK_WALLET_MANAGER_STATE_CONNECTED
        });
    }

    if (needLock) pthread_mutex_unlock(&qry->lock);
}

extern void
wkClientQRYManagerTickTock (WKClientQRYManager qry) {
    pthread_mutex_lock (&qry->lock);

    // Only continue if connected
    if (qry->connected) {
        switch (qry->manager->syncMode) {
            case WK_SYNC_MODE_API_ONLY:
            case WK_SYNC_MODE_API_WITH_P2P_SEND:
                // Alwwys get the current block
                wkClientQRYRequestBlockNumber (qry);
                break;

            case WK_SYNC_MODE_P2P_WITH_API_SYNC:
            case WK_SYNC_MODE_P2P_ONLY:
                break;
        }
    }

    pthread_mutex_unlock (&qry->lock);
}

extern void
wkClientQRYManagerTickTockReceiveAddressSync (WKClientQRYManager qry) { // ADDED
    pthread_mutex_lock (&qry->lock);

    // Only continue if connected
    if (qry->connected) {
        switch (qry->manager->syncMode) {
            case WK_SYNC_MODE_API_ONLY:
            case WK_SYNC_MODE_API_WITH_P2P_SEND:
                // Alwwys get the current block
                wkClientQRYRequestBlockNumberReceiveAddressSync (qry);
                break;

            case WK_SYNC_MODE_P2P_WITH_API_SYNC:
            case WK_SYNC_MODE_P2P_ONLY:
                break;
        }
    }

    pthread_mutex_unlock (&qry->lock);
}

static void
wkClientQRYRequestSync (WKClientQRYManager qry, bool needLock) {
    if (needLock) pthread_mutex_lock (&qry->lock);

    // If we've successfully completed a sync then update `begBlockNumber` which will always be
    // `blockNumberOffset` prior to the `endBlockNumber`.  Using the offset, which is weeks prior,
    // ensures that we don't miss a range of blocks in the {transfer,transaction} query.

    if (qry->sync.completed && qry->sync.success) {

        // Be careful to ensure that the `begBlockNumber` is not negative.
        qry->sync.begBlockNumber = (qry->sync.endBlockNumber >= qry->blockNumberOffset
                                    ? qry->sync.endBlockNumber - qry->blockNumberOffset
                                    : 0);
    }

    // Whether or not we completed , update the `endBlockNumber` to the current block height.
    qry->sync.endBlockNumber = MAX (wkClientQRYGetNetworkBlockHeight (qry),
                                    qry->sync.begBlockNumber);

    // We'll update transactions if there are more blocks to examine and if the prior sync
    // completed (successfully or not).
    if (qry->sync.completed && qry->sync.begBlockNumber != qry->sync.endBlockNumber) {

        // Save the current requestId and mark the sync as completed successfully.
        qry->sync.rid = qry->requestId++;

        // Mark the sync as completed, unsucessfully (the initial state)
        wkClientQRYManagerUpdateSync (qry, false, false, false);

        // Get the addresses for the manager's wallet
        WKWallet wallet = wkWalletManagerGetWallet (qry->manager);
        BRSetOf(WKAddress) addresses = wkWalletGetAddressesForRecovery (wallet);
        assert (0 != BRSetCount(addresses));

        // We'll force the 'client' to return all transactions w/o regard to the `endBlockNumber`
        // Doing this ensures that the initial 'full-sync' returns everything.  Thus there is no
        // need to wait for a future 'tick tock' to get the recent and pending transactions'.  For
        // BTC the future 'tick tock' is minutes away; which is a burden on Users as they wait.

        wkClientQRYRequestTransactionsOrTransfers (qry,
                                                       (WK_CLIENT_REQUEST_USE_TRANSFERS == qry->byType
                                                        ? CLIENT_CALLBACK_REQUEST_TRANSFERS
                                                        : CLIENT_CALLBACK_REQUEST_TRANSACTIONS),
                                                       NULL,
                                                       addresses,
                                                       qry->sync.rid);

        wkWalletGive (wallet);
    }

    if (needLock) pthread_mutex_unlock (&qry->lock);
}

static void
wkClientQRYRequestReceiveAddressSync (WKClientQRYManager qry, bool needLock) {
    if (needLock) pthread_mutex_lock (&qry->lock);

    // If we've successfully completed a sync then update `begBlockNumber` which will always be
    // `blockNumberOffset` prior to the `endBlockNumber`.  Using the offset, which is weeks prior,
    // ensures that we don't miss a range of blocks in the {transfer,transaction} query.

    if (qry->sync.completed && qry->sync.success) {

        // Be careful to ensure that the `begBlockNumber` is not negative.
        qry->sync.begBlockNumber = (qry->sync.endBlockNumber >= qry->blockNumberOffset
                                    ? qry->sync.endBlockNumber - qry->blockNumberOffset
                                    : 0);
    }

    // Whether or not we completed , update the `endBlockNumber` to the current block height.
    qry->sync.endBlockNumber = MAX (wkClientQRYGetNetworkBlockHeight (qry),
                                    qry->sync.begBlockNumber);

    // We'll update transactions if there are more blocks to examine and if the prior sync
    // completed (successfully or not).
    if (qry->sync.completed && qry->sync.begBlockNumber != qry->sync.endBlockNumber) {

        // Save the current requestId and mark the sync as completed successfully.
        qry->sync.rid = qry->requestId++;

        // Mark the sync as completed, unsucessfully (the initial state)
//        wkClientQRYManagerUpdateSync (qry, false, false, false);

        // Get the addresses for the manager's wallet
        WKWallet wallet = wkWalletManagerGetWallet (qry->manager);
        WKAddress address = wkWalletGetAddress (wallet, qry->manager->addressScheme);
        BRSetOf(WKAddress) addresses = wkAddressSetCreate (1);
        BRSetAdd (addresses, address);
        assert (0 != BRSetCount(addresses));

        // We'll force the 'client' to return all transactions w/o regard to the `endBlockNumber`
        // Doing this ensures that the initial 'full-sync' returns everything.  Thus there is no
        // need to wait for a future 'tick tock' to get the recent and pending transactions'.  For
        // BTC the future 'tick tock' is minutes away; which is a burden on Users as they wait.

        wkClientQRYRequestTransactionsOrTransfers (qry,
                                                       (WK_CLIENT_REQUEST_USE_TRANSFERS == qry->byType
                                                        ? CLIENT_CALLBACK_REQUEST_TRANSFERS
                                                        : CLIENT_CALLBACK_REQUEST_TRANSACTIONS),
                                                       NULL,
                                                       addresses,
                                                       qry->sync.rid);

        wkWalletGive (wallet);
    }

    if (needLock) pthread_mutex_unlock (&qry->lock);
}

// MARK: - Client Callback State

static WKClientCallbackState
wkClientCallbackStateCreate (WKClientCallbackType type,
                                 size_t rid) {
    WKClientCallbackState state = calloc (1, sizeof (struct WKClientCallbackStateRecord));

    state->type = type;
    state->rid  = rid;

    return state;
}

static WKClientCallbackState
wkClientCallbackStateCreateGetTrans (WKClientCallbackType type,
                                         OwnershipGiven BRSetOf(WKAddress) addresses,
                                         size_t rid) {
    assert (CLIENT_CALLBACK_REQUEST_TRANSFERS    == type ||
            CLIENT_CALLBACK_REQUEST_TRANSACTIONS == type);
    WKClientCallbackState state = wkClientCallbackStateCreate (type, rid);

    switch (type) {
        case CLIENT_CALLBACK_REQUEST_TRANSFERS:
            state->u.getTransfers.addresses = addresses;
            break;
        case CLIENT_CALLBACK_REQUEST_TRANSACTIONS:
            state->u.getTransactions.addresses = addresses;
            break;
        default:
            assert (0);
            break;
    }

    return state;
}

static WKClientCallbackState
wkClientCallbackStateCreateSubmitTransaction (WKWallet wallet,
                                                  WKTransfer transfer,
                                                  size_t rid) {
    WKClientCallbackState state = wkClientCallbackStateCreate (CLIENT_CALLBACK_SUBMIT_TRANSACTION, rid);

    state->u.submitTransaction.wallet   = wkWalletTake   (wallet);
    state->u.submitTransaction.transfer = wkTransferTake (transfer);

    return state;
}

static WKClientCallbackState
wkClientCallbackStateCreateEstimateTransactionFee (WKHash hash,
                                                   WKCookie cookie,
                                                   OwnershipKept WKTransfer transfer,
                                                   OwnershipKept WKNetworkFee networkFee,
                                                   size_t rid) {
    WKClientCallbackState state = wkClientCallbackStateCreate (CLIENT_CALLBACK_ESTIMATE_TRANSACTION_FEE, rid);

    state->u.estimateTransactionFee.cookie     = cookie;
    state->u.estimateTransactionFee.transfer   = wkTransferTake   (transfer);
    state->u.estimateTransactionFee.networkFee = wkNetworkFeeTake (networkFee);

    return state;
}

static void
wkClientCallbackStateRelease (WKClientCallbackState state) {
    switch (state->type) {
        case CLIENT_CALLBACK_REQUEST_TRANSFERS:
            wkAddressSetRelease (state->u.getTransfers.addresses);
            break;

        case CLIENT_CALLBACK_REQUEST_TRANSACTIONS:
            wkAddressSetRelease(state->u.getTransactions.addresses);
            break;

        case CLIENT_CALLBACK_SUBMIT_TRANSACTION:
            wkWalletGive   (state->u.submitTransaction.wallet);
            wkTransferGive (state->u.submitTransaction.transfer);
            break;

        case CLIENT_CALLBACK_ESTIMATE_TRANSACTION_FEE:
            wkHashGive (state->u.estimateTransactionFee.hash);
            wkTransferGive(state->u.estimateTransactionFee.transfer);
            wkNetworkFeeGive (state->u.estimateTransactionFee.networkFee);
            break;

        default:
            break;
    }

    memset (state, 0, sizeof (struct WKClientCallbackStateRecord));
    free (state);
}


// MARK: - Request/Announce Block Number

typedef struct {
    BREvent base;
    WKWalletManager manager;
    WKClientCallbackState callbackState;
    WKBlockNumber blockNumber;
    char * blockHashString;
    WKClientError error;
} WKClientAnnounceBlockNumberEvent;

static void
wkClientHandleBlockNumber (OwnershipKept WKWalletManager manager,
                           OwnershipGiven WKClientCallbackState callbackState,
                           WKBlockNumber blockNumber,
                           char *blockHashString,
                           OwnershipGiven WKClientError error) {

    if (NULL == error) {
        WKBlockNumber oldBlockNumber = wkNetworkGetHeight (manager->network);

        if (oldBlockNumber != blockNumber) {
            wkNetworkSetHeight (manager->network, blockNumber);

            if (NULL != blockHashString && '\0' != blockHashString[0]) {
                WKHash verifiedBlockHash = wkNetworkCreateHashFromString (manager->network, blockHashString);
                wkNetworkSetVerifiedBlockHash (manager->network, verifiedBlockHash);
                wkHashGive (verifiedBlockHash);
            }

            wkWalletManagerGenerateEvent (manager, (WKWalletManagerEvent) {
                WK_WALLET_MANAGER_EVENT_BLOCK_HEIGHT_UPDATED,
                { .blockHeight = blockNumber }
            });
        }

        wkClientCallbackStateRelease (callbackState);
        wkMemoryFree(blockHashString);
    }

    // After getting any block number, whether successful or not, do a sync.  The sync will be
    // incremental or full - depending on where the last sync ended.
    wkClientQRYRequestSync (manager->qryManager, true);

    wkWalletManagerAnnounceClientError (manager, error);
}

static void
wkClientHandleBlockNumberReceiveAddressSync (OwnershipKept WKWalletManager manager,
                           OwnershipGiven WKClientCallbackState callbackState,
                           WKBlockNumber blockNumber,
                           char *blockHashString,
                           OwnershipGiven WKClientError error) {

    if (NULL == error) {
        WKBlockNumber oldBlockNumber = wkNetworkGetHeight (manager->network);

        if (oldBlockNumber != blockNumber) {
            wkNetworkSetHeight (manager->network, blockNumber);

            if (NULL != blockHashString && '\0' != blockHashString[0]) {
                WKHash verifiedBlockHash = wkNetworkCreateHashFromString (manager->network, blockHashString);
                wkNetworkSetVerifiedBlockHash (manager->network, verifiedBlockHash);
                wkHashGive (verifiedBlockHash);
            }

            wkWalletManagerGenerateEvent (manager, (WKWalletManagerEvent) {
                WK_WALLET_MANAGER_EVENT_BLOCK_HEIGHT_UPDATED,
                { .blockHeight = blockNumber }
            });
        }

        wkClientCallbackStateRelease (callbackState);
        wkMemoryFree(blockHashString);
    }

    // After getting any block number, whether successful or not, do a sync.  The sync will be
    // incremental or full - depending on where the last sync ended.
//    wkClientQRYRequestReceiveAddressSync (manager->qryManager, true);

    wkWalletManagerAnnounceClientError (manager, error);
}

static void
wkClientAnnounceBlockNumberDispatcher (BREventHandler ignore,
                                       WKClientAnnounceBlockNumberEvent *event) {
    wkClientHandleBlockNumber (event->manager,
                               event->callbackState,
                               event->blockNumber,
                               event->blockHashString,
                               event->error);
}

static void
wkClientReceiveAddressSyncBlockNumberDispatcher (BREventHandler ignore,
                                       WKClientAnnounceBlockNumberEvent *event) {
    wkClientHandleBlockNumberReceiveAddressSync (event->manager,
                               event->callbackState,
                               event->blockNumber,
                               event->blockHashString,
                               event->error);
}

static void
wkClientAnnounceBlockNumberDestroyer (WKClientAnnounceBlockNumberEvent *event) {
    wkWalletManagerGive (event->manager);
    wkClientCallbackStateRelease (event->callbackState);
    wkMemoryFree (event->blockHashString);
}

BREventType handleClientAnnounceBlockNumberEventType = {
    "CWM: Handle Client Announce Block Number Event",
    sizeof (WKClientAnnounceBlockNumberEvent),
    (BREventDispatcher) wkClientAnnounceBlockNumberDispatcher,
    (BREventDestroyer) wkClientAnnounceBlockNumberDestroyer
};

BREventType handleClientReceiveAddressSyncBlockNumberEventType = {
    "CWM: Handle Client Receive Address Sync Block Number Event",
    sizeof (WKClientAnnounceBlockNumberEvent),
    (BREventDispatcher) wkClientReceiveAddressSyncBlockNumberDispatcher,
    (BREventDestroyer)  wkClientAnnounceBlockNumberDestroyer
};

extern void
wkClientAnnounceBlockNumberSuccess (OwnershipKept WKWalletManager cwm,
                                    OwnershipGiven WKClientCallbackState callbackState,
                                    WKBlockNumber blockNumber,
                                    const char *blockHashString) {
    WKClientAnnounceBlockNumberEvent event =
    { { NULL, &handleClientAnnounceBlockNumberEventType },
        wkWalletManagerTakeWeak(cwm),
        callbackState,
        blockNumber,
        (NULL == blockHashString ? NULL : strdup (blockHashString)),
        NULL
    };

    eventHandlerSignalEvent (cwm->handler, (BREvent *) &event);
}

extern void
wkClientAnnounceBlockNumberFailure (OwnershipKept WKWalletManager cwm,
                                    OwnershipGiven WKClientCallbackState callbackState,
                                    OwnershipGiven WKClientError error) {
    WKClientAnnounceBlockNumberEvent event =
    { { NULL, &handleClientAnnounceBlockNumberEventType },
        wkWalletManagerTakeWeak(cwm),
        callbackState,
        BLOCK_NUMBER_UNKNOWN,
        NULL,
        error };

    eventHandlerSignalEvent (cwm->handler, (BREvent *) &event);
}

extern void
wkClientReceiveAddressSyncBlockNumberSuccess (OwnershipKept WKWalletManager cwm,
                                    OwnershipGiven WKClientCallbackState callbackState,
                                    WKBlockNumber blockNumber,
                                    const char *blockHashString) {
    WKClientAnnounceBlockNumberEvent event =
    { { NULL, &handleClientReceiveAddressSyncBlockNumberEventType },
        wkWalletManagerTakeWeak(cwm),
        callbackState,
        blockNumber,
        (NULL == blockHashString ? NULL : strdup (blockHashString)),
        NULL
    };

    eventHandlerSignalEvent (cwm->handler, (BREvent *) &event);
}

extern void
wkClientReceiveAddressSyncBlockNumberFailure (OwnershipKept WKWalletManager cwm,
                                    OwnershipGiven WKClientCallbackState callbackState,
                                    OwnershipGiven WKClientError error) {
    WKClientAnnounceBlockNumberEvent event =
    { { NULL, &handleClientReceiveAddressSyncBlockNumberEventType },
        wkWalletManagerTakeWeak(cwm),
        callbackState,
        BLOCK_NUMBER_UNKNOWN,
        NULL,
        error };

    eventHandlerSignalEvent (cwm->handler, (BREvent *) &event);
}

static void
wkClientQRYRequestBlockNumber (WKClientQRYManager qry) {
    // Extract CWM, checking to make sure it still lives
    WKWalletManager cwm = wkWalletManagerTakeWeak(qry->manager);
    if (NULL == cwm) return;

    WKClientCallbackState callbackState = wkClientCallbackStateCreate (CLIENT_CALLBACK_REQUEST_BLOCK_NUMBER,
                                                                                 qry->requestId++);

    qry->client.funcGetBlockNumber (qry->client.context,
                                    wkWalletManagerTake (cwm),
                                    callbackState);

    wkWalletManagerGive (cwm);
}

static void
wkClientQRYRequestBlockNumberReceiveAddressSync (WKClientQRYManager qry) { // ADDED
    // Extract CWM, checking to make sure it still lives
    WKWalletManager cwm = wkWalletManagerTakeWeak(qry->manager);
    if (NULL == cwm) return;

    WKClientCallbackState callbackState = wkClientCallbackStateCreate (CLIENT_CALLBACK_REQUEST_BLOCK_NUMBER,
                                                                                 qry->requestId++);

    qry->client.funcGetBlockNumberReceiveAddressSync (qry->client.context,
                                    wkWalletManagerTake (cwm),
                                    callbackState);

    wkWalletManagerGive (cwm);
}

// MARK: - Request/Announce Transactions

typedef struct {
    BREvent base;
    WKWalletManager manager;
    WKClientCallbackState callbackState;
    BRArrayOf (WKClientTransactionBundle) bundles;
    WKClientError error;
} WKClientAnnounceTransactionsEvent;

extern void
wkClientHandleTransactions (OwnershipKept WKWalletManager manager,
                            OwnershipGiven WKClientCallbackState callbackState,
                            BRArrayOf (WKClientTransactionBundle) bundles,
                            OwnershipGiven WKClientError error) {

    WKClientQRYManager qry = manager->qryManager;

    pthread_mutex_lock (&qry->lock);
    bool matchedRids = (callbackState->rid == qry->sync.rid);
    pthread_mutex_unlock (&qry->lock);

    bool syncCompleted = false;
    bool syncSuccess   = false;

    // Process the results if the bundles are for our rid; otherwise simply discard;
    if (matchedRids) {
        if (NULL == error) {
            size_t bundlesCount = array_count(bundles);

            // Save the transaction bundles immediately
            for (size_t index = 0; index < bundlesCount; index++)
                wkWalletManagerSaveTransactionBundle(manager, bundles[index]);

            // Sort bundles to have the lowest blocknumber first.  Use of `mergesort` is
            // appropriate given that the bundles are likely already ordered.  This minimizes
            // dependency resolution between later transactions depending on prior transactions.
            //
            // Seems that there may be duplicates in `bundles`; will be dealt with later

            mergesort_brd (bundles, bundlesCount, sizeof (WKClientTransactionBundle),
                           wkClientTransactionBundleCompareForSort);

            // Recover transfers from each bundle
            for (size_t index = 0; index < bundlesCount; index++)
                wkWalletManagerRecoverTransfersFromTransactionBundle (manager, bundles[index]);

            // The following assumes `bundles` has produced transfers which may have
            // impacted the wallet's addresses.  Thus the recovery must be *serial w.r.t. the
            // subsequent call to `wkClientQRYRequestTransactionsOrTransfers()`.

            WKWallet wallet = wkWalletManagerGetWallet(manager);

            // We've completed a query for `oldAddresses`
            BRSetOf(WKAddress) oldAddresses = callbackState->u.getTransactions.addresses;

            // We'll need another query if `newAddresses` is now larger then `oldAddresses`
            BRSetOf(WKAddress) newAddresses = wkWalletGetAddressesForRecovery (wallet);

            // Make the actual request; if none is needed, then we are done
            if (!wkClientQRYRequestTransactionsOrTransfers (qry,
                                                            CLIENT_CALLBACK_REQUEST_TRANSACTIONS,
                                                            oldAddresses,
                                                            newAddresses,
                                                            callbackState->rid)) {
                syncCompleted = true;
                syncSuccess   = true;
            }

            wkWalletGive (wallet);
        }
        else {
            syncCompleted = true;
            syncSuccess   = false;
        }
    }

    wkClientQRYManagerUpdateSync (qry, syncCompleted, syncSuccess, true);

    if (NULL != bundles) array_free_all (bundles, wkClientTransactionBundleRelease);
    wkClientCallbackStateRelease(callbackState);

    wkWalletManagerAnnounceClientError (manager, error);
}

static void
wkClientAnnounceTransactionsDispatcher (BREventHandler ignore,
                                            WKClientAnnounceTransactionsEvent *event) {
    wkClientHandleTransactions (event->manager,
                                event->callbackState,
                                event->bundles,
                                event->error);
}

static void
wkClientAnnounceTransactionsDestroyer (WKClientAnnounceTransactionsEvent *event) {
    wkWalletManagerGive (event->manager);
    wkClientCallbackStateRelease (event->callbackState);
    if (NULL != event->bundles) array_free_all (event->bundles, wkClientTransactionBundleRelease);
}

BREventType handleClientAnnounceTransactionsEventType = {
    "CWM: Handle Client Announce Transactions Event",
    sizeof (WKClientAnnounceTransactionsEvent),
    (BREventDispatcher) wkClientAnnounceTransactionsDispatcher,
    (BREventDestroyer) wkClientAnnounceTransactionsDestroyer
};

extern void
wkClientAnnounceTransactionsSuccess (OwnershipKept WKWalletManager manager,
                                     OwnershipGiven WKClientCallbackState callbackState,
                                     WKClientTransactionBundle *bundles,  // given elements, not array
                                     size_t bundlesCount) {
    BRArrayOf (WKClientTransactionBundle) eventBundles;
    array_new (eventBundles, bundlesCount);
    array_add_array (eventBundles, bundles, bundlesCount);

    WKClientAnnounceTransactionsEvent event =
    { { NULL, &handleClientAnnounceTransactionsEventType },
        wkWalletManagerTakeWeak(manager),
        callbackState,
        eventBundles,
        NULL };

    eventHandlerSignalEvent (manager->handler, (BREvent *) &event);
}

extern void
wkClientAnnounceTransactionsFailure (OwnershipKept WKWalletManager manager,
                                     OwnershipGiven WKClientCallbackState callbackState,
                                     OwnershipGiven WKClientError error) {
    WKClientAnnounceTransactionsEvent event =
    { { NULL, &handleClientAnnounceTransactionsEventType },
        wkWalletManagerTakeWeak(manager),
        callbackState,
        NULL,
        error };

    eventHandlerSignalEvent (manager->handler, (BREvent *) &event);
}

// MARK: - Announce Transfer

typedef struct {
    BREvent base;
    WKWalletManager manager;
    WKClientCallbackState callbackState;
    BRArrayOf (WKClientTransferBundle) bundles;
    WKClientError error;
} WKClientAnnounceTransfersEvent;

extern void
wkClientHandleTransfers (OwnershipKept WKWalletManager manager,
                         OwnershipGiven WKClientCallbackState callbackState,
                         BRArrayOf (WKClientTransferBundle) bundles,
                         OwnershipGiven WKClientError error) {

    WKClientQRYManager qry = manager->qryManager;

    pthread_mutex_lock (&qry->lock);
    bool matchedRids = (callbackState->rid == qry->sync.rid);
    pthread_mutex_unlock (&qry->lock);

    bool syncCompleted = false;
    bool syncSuccess   = false;

    // Process the results if the bundles are for our rid; otherwise simply discard;
    if (matchedRids) {
        if (NULL == error) {
            size_t bundlesCount = array_count(bundles);

            for (size_t index = 0; index < bundlesCount; index++)
                wkWalletManagerSaveTransferBundle(manager, bundles[index]);

            // Sort bundles to have the lowest blocknumber first.  Use of `mergesort` is
            // appropriate given that the bundles are likely already ordered.  This minimizes
            // dependency resolution between later transfers depending on prior transfers.

            mergesort_brd (bundles, bundlesCount, sizeof (WKClientTransferBundle),
                           wkClientTransferBundleCompareForSort);

            // Recover transfers from each bundle
            for (size_t index = 0; index < bundlesCount; index++)
                wkWalletManagerRecoverTransferFromTransferBundle (manager, bundles[index]);

            WKWallet wallet = wkWalletManagerGetWallet(manager);

            // We've completed a query for `oldAddresses`
            BRSetOf(WKAddress) oldAddresses = callbackState->u.getTransactions.addresses;

            // We'll need another query if `newAddresses` is now larger then `oldAddresses`
            BRSetOf(WKAddress) newAddresses = wkWalletGetAddressesForRecovery (wallet);

            // Make the actual request; if none is needed, then we are done.  Use the
            // same `rid` as we are in the same sync.
            if (!wkClientQRYRequestTransactionsOrTransfers (qry,
                                                            CLIENT_CALLBACK_REQUEST_TRANSFERS,
                                                            oldAddresses,
                                                            newAddresses,
                                                            callbackState->rid)) {
                syncCompleted = true;
                syncSuccess   = true;
            }

            wkWalletGive (wallet);
        }

        else {
            syncCompleted = true;
            syncSuccess   = false;
        }
    }

    wkClientQRYManagerUpdateSync (qry, syncCompleted, syncSuccess, true);

    if (NULL != bundles) array_free_all (bundles, wkClientTransferBundleRelease);
    wkClientCallbackStateRelease(callbackState);

    wkWalletManagerAnnounceClientError (manager, error);
}

static void
wkClientAnnounceTransfersDispatcher (BREventHandler ignore,
                                     WKClientAnnounceTransfersEvent *event) {
    wkClientHandleTransfers (event->manager,
                             event->callbackState,
                             event->bundles,
                             event->error);
}

static void
wkClientAnnounceTransfersDestroyer (WKClientAnnounceTransfersEvent *event) {
    wkWalletManagerGive (event->manager);
    wkClientCallbackStateRelease (event->callbackState);
    if (NULL != event->bundles) array_free_all (event->bundles, wkClientTransferBundleRelease);
}

BREventType handleClientAnnounceTransfersEventType = {
    "CWM: Handle Client Announce Transfers Event",
    sizeof (WKClientAnnounceTransfersEvent),
    (BREventDispatcher) wkClientAnnounceTransfersDispatcher,
    (BREventDestroyer) wkClientAnnounceTransfersDestroyer
};


extern void
wkClientAnnounceTransfersSuccess (OwnershipKept WKWalletManager manager,
                                  OwnershipGiven WKClientCallbackState callbackState,
                                  OwnershipGiven WKClientTransferBundle *bundles, // given elements, not array
                                  size_t bundlesCount) {
    BRArrayOf (WKClientTransferBundle) eventBundles;
    array_new (eventBundles, bundlesCount);
    array_add_array (eventBundles, bundles, bundlesCount);

    WKClientAnnounceTransfersEvent event =
    { { NULL, &handleClientAnnounceTransfersEventType },
        wkWalletManagerTakeWeak(manager),
        callbackState,
        eventBundles,
        NULL };

    eventHandlerSignalEvent (manager->handler, (BREvent *) &event);
}

extern void
wkClientAnnounceTransfersFailure (OwnershipKept WKWalletManager manager,
                                  OwnershipGiven WKClientCallbackState callbackState,
                                  OwnershipGiven WKClientError error) {
    WKClientAnnounceTransfersEvent event =
    { { NULL, &handleClientAnnounceTransfersEventType },
        wkWalletManagerTakeWeak(manager),
        callbackState,
        NULL,
        error };

    eventHandlerSignalEvent (manager->handler, (BREvent *) &event);
}

// MARK: - Request Transactions/Transfers

static BRArrayOf(char *)
wkClientQRYGetAddresses (WKClientQRYManager qry,
                             OwnershipKept BRSetOf(WKAddress) addresses) {
    BRArrayOf(char *) addressesEncoded;
    array_new (addressesEncoded, BRSetCount (addresses));

    FOR_SET (WKAddress, address, addresses) {
        array_add (addressesEncoded, wkAddressAsString (address));
    }

    return addressesEncoded;
}

static void
wkClientQRYReleaseAddresses (BRArrayOf(char *) addresses) {
    for (size_t index = 0; index < array_count(addresses); index++)
    free (addresses[index]);
    array_free (addresses);
}

static bool
wkClientQRYRequestTransactionsOrTransfers (WKClientQRYManager qry,
                                               WKClientCallbackType type,
                                               OwnershipKept  BRSetOf(WKAddress) oldAddresses,
                                               OwnershipGiven BRSetOf(WKAddress) newAddresses,
                                               size_t requestId) {

    WKWalletManager manager = wkWalletManagerTakeWeak(qry->manager);
    if (NULL == manager) {
        wkAddressSetRelease(newAddresses);
        return false;
    }

    // Determine the set of addresses needed as `newAddresses - oldAddresses`.  The elements in
    // `addresses` ARE NOT owned by `addresses`; they remain owned by `newAddresses`
    BRSetOf(WKAddress) addresses = BRSetCopy (newAddresses, NULL);

    if (NULL != oldAddresses)
        BRSetMinus (addresses, oldAddresses);

    // If there are `addresses` then a reqeust is needed.
    bool needRequest = BRSetCount (addresses) > 0;

    if (needRequest) {
        // Get an array of the remaining, needed `addresses`
        BRArrayOf(char *) addressesEncoded = wkClientQRYGetAddresses (qry, addresses);

        // Create a `calllbackState`; importantly, report `newAddress` as the accumulated addresses
        // that have been requested.  Note, this specific request will be for `addresses` only.
        // The elements in `newAddresses` are now owned by `callbackState`.
        WKClientCallbackState callbackState = wkClientCallbackStateCreateGetTrans (type,
                                                                                             newAddresses,
                                                                                             requestId);

        switch (type) {
            case CLIENT_CALLBACK_REQUEST_TRANSFERS:
                qry->client.funcGetTransfers (qry->client.context,
                                              wkWalletManagerTake(manager),
                                              callbackState,
                                              (const char **) addressesEncoded,
                                              array_count(addressesEncoded),
                                              qry->sync.begBlockNumber,
                                              (qry->sync.unbounded
                                               ? BLOCK_HEIGHT_UNBOUND_VALUE
                                               : qry->sync.endBlockNumber));
                break;

            case CLIENT_CALLBACK_REQUEST_TRANSACTIONS:
                qry->client.funcGetTransactions (qry->client.context,
                                                 wkWalletManagerTake(manager),
                                                 callbackState,
                                                 (const char **) addressesEncoded,
                                                 array_count(addressesEncoded),
                                                 qry->sync.begBlockNumber,
                                                 (qry->sync.unbounded
                                                  ? BLOCK_HEIGHT_UNBOUND_VALUE
                                                  : qry->sync.endBlockNumber));
                break;

            default:
                assert (false);
        }

        wkClientQRYReleaseAddresses (addressesEncoded);
    }
    else {
        // If `newAddresses` ownership was not transfered to `callbackState`, then release everything.
        wkAddressSetRelease (newAddresses);
    }

    wkWalletManagerGive (manager);
    BRSetFree (addresses);

    return needRequest;
}


// MARK: Announce Submit Transfer

typedef struct {
    BREvent base;
    WKWalletManager manager;
    WKClientCallbackState callbackState;
    char *identifier;
    char *hash;
    WKClientError error;
} WKClientAnnounceSubmitEvent;

static WKTransferSubmitError
wkClientErrorToSubmitError (WKWalletManager manager,
                            WKClientError clientError) {
    static WKTransferSubmitErrorType types[NUMBER_OF_TRANSFER_SUBMIT_ERROR_TYPES] = {
        WK_TRANSFER_SUBMIT_ERROR_CLIENT_BAD_REQUEST,
        WK_TRANSFER_SUBMIT_ERROR_CLIENT_PERMISSION,
        WK_TRANSFER_SUBMIT_ERROR_CLIENT_RESOURCE,
        WK_TRANSFER_SUBMIT_ERROR_CLIENT_BAD_RESPONSE,
        WK_TRANSFER_SUBMIT_ERROR_UNKNOWN,
        WK_TRANSFER_SUBMIT_ERROR_CLIENT_UNAVAILABLE,
        WK_TRANSFER_SUBMIT_ERROR_LOST_CONNECTIVITY,
    };

    WKTransferSubmitErrorType type = (WK_CLIENT_ERROR_SUBMISSION == clientError->type
                                      ? clientError->u.submitErrorType
                                      : types[clientError->type]);

    return wkTransferSubmitErrorCreate (type, clientError->details);
}

static void
wkClientHandleSubmit (OwnershipKept WKWalletManager manager,
                      OwnershipGiven WKClientCallbackState callbackState,
                      OwnershipKept char *identifierStr,
                      OwnershipKept char *hashStr,
                      OwnershipGiven WKClientError error) {
    assert (CLIENT_CALLBACK_SUBMIT_TRANSACTION == callbackState->type);

    WKWallet   wallet   = callbackState->u.submitTransaction.wallet;
    WKTransfer transfer = callbackState->u.submitTransaction.transfer;

    // Get the transfer state
    WKTransferState transferState = (NULL == error
                                     ? wkTransferStateInit (WK_TRANSFER_STATE_SUBMITTED)
                                     : wkTransferStateErroredInit (wkClientErrorToSubmitError (manager, error)));

    // Recover the `state` as either SUBMITTED or a UNKNOWN ERROR.  We have a slight issue, as
    // a possible race condition, whereby the transfer can already be INCLUDED by the time this
    // `announce` is called.  That has got to be impossible right?
    //
    // Assign the state; generate events in the process.
    wkTransferSetState (transfer, transferState);

    // On successful submit, the hash might be determined.  Yes, somewhat unfathomably (HBAR)
    WKHash hash = (NULL == hashStr ? NULL : wkNetworkCreateHashFromString (manager->network, hashStr));

    if (NULL != hash) {
        WKBoolean hashChanged = wkTransferSetHash (transfer, hash);

        if (WK_TRUE == hashChanged) {
            WKTransferState state = wkTransferGetState(transfer);

            wkTransferGenerateEvent (transfer, (WKTransferEvent) {
                WK_TRANSFER_EVENT_CHANGED,
                { .state = {
                    wkTransferStateTake (state),
                    wkTransferStateTake (state) }}
            });

            wkTransferStateGive (state);
        }

        wkHashGive (hash);
    }

    // If the manager's wallet (aka the primaryWallet) does not match the wallet, then the
    // transaction fee must be updated... on an ERROR - it was already updated upon submit, so
    // only on ERROR do we undo the fee.
    if (wallet != manager->wallet &&
        WK_TRANSFER_STATE_ERRORED == transferState->type &&
        WK_TRANSFER_RECEIVED      != transfer->direction) {
        wkWalletUpdBalance (manager->wallet, true);
    }

    wkTransferStateGive(transferState);

    wkWalletManagerGive (manager);
    wkClientCallbackStateRelease(callbackState);
    wkMemoryFree (identifierStr);
    wkMemoryFree (hashStr);

    wkWalletManagerAnnounceClientError (manager, error);
}

static void
wkClientAnnounceSubmitDispatcher (BREventHandler ignore,
                                  WKClientAnnounceSubmitEvent *event) {
    wkClientHandleSubmit (event->manager,
                          event->callbackState,
                          event->identifier,
                          event->hash,
                          event->error);
}

static void
wkClientAnnounceSubmitDestroyer (WKClientAnnounceSubmitEvent *event) {
    wkWalletManagerGive (event->manager);
    wkClientCallbackStateRelease (event->callbackState);
    wkMemoryFree (event->identifier);
    wkMemoryFree (event->hash);
}

BREventType handleClientAnnounceSubmitEventType = {
    "CWM: Handle Client Announce Submit Event",
    sizeof (WKClientAnnounceSubmitEvent),
    (BREventDispatcher) wkClientAnnounceSubmitDispatcher,
    (BREventDestroyer) wkClientAnnounceSubmitDestroyer
};

extern void
wkClientAnnounceSubmitTransferSuccess (OwnershipKept WKWalletManager manager,
                                       OwnershipGiven WKClientCallbackState callbackState,
                                       OwnershipKept const char *identifier,
                                       OwnershipKept const char *hash) {
    WKClientAnnounceSubmitEvent event =
    { { NULL, &handleClientAnnounceSubmitEventType },
        wkWalletManagerTakeWeak(manager),
        callbackState,
        (NULL == identifier ? NULL : strdup(identifier)),
        (NULL == hash ? NULL : strdup (hash)),
        NULL };

    eventHandlerSignalEvent (manager->handler, (BREvent *) &event);
}

extern void
wkClientAnnounceSubmitTransferFailure (OwnershipKept WKWalletManager manager,
                                       OwnershipGiven WKClientCallbackState callbackState,
                                       OwnershipGiven WKClientError error) {
    WKClientAnnounceSubmitEvent event =
    { { NULL, &handleClientAnnounceSubmitEventType },
        wkWalletManagerTakeWeak(manager),
        callbackState,
        NULL,
        NULL,
        error };

    eventHandlerSignalEvent (manager->handler, (BREvent *) &event);
}

static void
wkClientQRYSubmitTransfer (WKClientQRYManager qry,
                               WKWallet   wallet,
                               WKTransfer transfer) {
    WKWalletManager manager = wkWalletManagerTakeWeak(qry->manager);
    if (NULL == manager) return;

    size_t   serializationCount;
    uint8_t *serialization = wkTransferSerializeForSubmission (transfer,
                                                                   manager->network,
                                                                   &serializationCount);

    WKClientCallbackState callbackState =
    wkClientCallbackStateCreateSubmitTransaction (wallet, transfer, qry->requestId++);

    qry->client.funcSubmitTransaction (qry->client.context,
                                       manager,
                                       callbackState,
                                       wkTransferGetIdentifier(transfer),
                                       wkTransferGetExchangeId(transfer),
                                       serialization,
                                       serializationCount);

    free (serialization);
}

// MARK: - Announce Estimate Transaction Fee

typedef struct {
    BREvent base;
    WKWalletManager manager;
    WKClientCallbackState callbackState;
    uint64_t costUnits;
    BRArrayOf(char*) keys;
    BRArrayOf(char*) vals;
    WKClientError error;
} WKClientAnnounceEstimateTransactionFeeEvent;

static void
wkClientHandleEstimateTransactionFee (OwnershipKept WKWalletManager manager,
                                      OwnershipGiven WKClientCallbackState callbackState,
                                      uint64_t costUnits,
                                      BRArrayOf(char*) attributeKeys,
                                      BRArrayOf(char*) attributeVals,
                                      OwnershipGiven WKClientError error) {
    assert (CLIENT_CALLBACK_ESTIMATE_TRANSACTION_FEE == callbackState->type);

    WKStatus status = (NULL == error ? WK_SUCCESS : WK_ERROR_FAILED);

    WKCookie     cookie     = callbackState->u.estimateTransactionFee.cookie;
    WKTransfer   transfer   = callbackState->u.estimateTransactionFee.transfer;
    WKNetworkFee networkFee = callbackState->u.estimateTransactionFee.networkFee;

    // Unused
    WKAmount pricePerCostFactor = wkNetworkFeeGetPricePerCostFactor (networkFee);

    // Leave a uint64_t?
    double costFactor = (double) costUnits;

    WKFeeBasis feeBasis = NULL;
    if (WK_SUCCESS == status)
        feeBasis = wkWalletManagerRecoverFeeBasisFromFeeEstimate (manager,
                                                                  transfer,
                                                                  networkFee,
                                                                  costFactor,
                                                                  array_count(attributeKeys),
                                                                  (const char**) attributeKeys,
                                                                  (const char**) attributeVals);

    wkWalletGenerateEvent (manager->wallet, wkWalletEventCreateFeeBasisEstimated(status, cookie, feeBasis));

    wkFeeBasisGive(feeBasis);

    // Unused
    wkAmountGive (pricePerCostFactor);

    wkClientCallbackStateRelease (callbackState);

    if (NULL != attributeKeys) array_free_all (attributeKeys, wkMemoryFree);
    if (NULL != attributeVals) array_free_all (attributeVals, wkMemoryFree);

    wkWalletManagerAnnounceClientError (manager, error);
}

static void
wkClientAnnounceEstimateTransactionFeeDispatcher (BREventHandler ignore,
                                                  WKClientAnnounceEstimateTransactionFeeEvent *event) {
    wkClientHandleEstimateTransactionFee (event->manager,
                                          event->callbackState,
                                          event->costUnits,
                                          event->keys,
                                          event->vals,
                                          event->error);
}

static void
wkClientAnnounceEstimateTransactionFeeDestroyer (WKClientAnnounceEstimateTransactionFeeEvent *event) {
    wkWalletManagerGive (event->manager);
    wkClientCallbackStateRelease (event->callbackState);
    if (NULL != event->keys) array_free_all (event->keys, wkMemoryFree);
    if (NULL != event->vals) array_free_all (event->vals, wkMemoryFree);
}

BREventType handleClientAnnounceEstimateTransactionFeeEventType = {
    "CWM: Handle Client Announce EstimateTransactionFee Event",
    sizeof (WKClientAnnounceEstimateTransactionFeeEvent),
    (BREventDispatcher) wkClientAnnounceEstimateTransactionFeeDispatcher,
    (BREventDestroyer) wkClientAnnounceEstimateTransactionFeeDestroyer
};

extern void
wkClientAnnounceEstimateTransactionFeeSuccess (OwnershipKept WKWalletManager manager,
                                            OwnershipGiven WKClientCallbackState callbackState,
                                            uint64_t costUnits,
                                            size_t attributesCount,
                                            OwnershipKept const char **attributeKeys,
                                            OwnershipKept const char **attributeVals) {
    BRArrayOf(char *) keys;
    array_new (keys, attributesCount);
    array_add_array (keys, (char**) attributeKeys, attributesCount);

    BRArrayOf(char *) vals;
    array_new (vals, attributesCount);
    array_add_array (vals, (char**) attributeVals, attributesCount);

    for (size_t index = 0; index < attributesCount; index++) {
        keys[index] = strdup (keys[index]);
        vals[index] = strdup (vals[index]);
    }

    WKClientAnnounceEstimateTransactionFeeEvent event =
    { { NULL, &handleClientAnnounceEstimateTransactionFeeEventType },
        wkWalletManagerTakeWeak(manager),
        callbackState,
        costUnits,
        keys,
        vals,
        NULL };

    eventHandlerSignalEvent (manager->handler, (BREvent *) &event);

}

extern void
wkClientAnnounceEstimateTransactionFeeFailure (OwnershipKept WKWalletManager manager,
                                               OwnershipGiven WKClientCallbackState callbackState,
                                               OwnershipGiven WKClientError error) {
    WKClientAnnounceEstimateTransactionFeeEvent event =
    { { NULL, &handleClientAnnounceEstimateTransactionFeeEventType },
        wkWalletManagerTakeWeak(manager),
        callbackState,
        0,
        NULL,
        NULL,
        error };

    eventHandlerSignalEvent (manager->handler, (BREvent *) &event);

}

extern void
wkClientQRYEstimateTransferFee (WKClientQRYManager qry,
                                    WKCookie   cookie,
                                    OwnershipKept WKTransfer transfer,
                                    OwnershipKept WKNetworkFee networkFee) {
    WKWalletManager manager = wkWalletManagerTakeWeak(qry->manager);
    if (NULL == manager) return;

    size_t   serializationCount;
    uint8_t *serialization = wkTransferSerializeForFeeEstimation (transfer, manager->network, &serializationCount);

    // There is no hash... transfer is not guaranteed to be signed; likely unsigned.
    WKHash hash      = NULL;
    const char  *hashAsHex = "";

    pthread_mutex_lock (&qry->lock);
    size_t rid = qry->requestId++;
    pthread_mutex_unlock (&qry->lock);

    // Provide the transfer itself.
    WKClientCallbackState callbackState = wkClientCallbackStateCreateEstimateTransactionFee (hash,
                                                                                             cookie,
                                                                                             transfer,     // taken
                                                                                             networkFee,   // taken
                                                                                             rid);

    qry->client.funcEstimateTransactionFee (qry->client.context,
                                            wkWalletManagerTake(manager),
                                            callbackState,
                                            serialization,
                                            serializationCount,
                                            hashAsHex);

    free (serialization);
}

// MARK: - Transfer Bundle

extern WKClientTransferBundle
wkClientTransferBundleCreate (WKTransferStateType status,
                              OwnershipKept const char */* transaction */ hash,
                              OwnershipKept const char */* transaction */ identifier,
                              OwnershipKept const char */* transfer */ uids,
                              OwnershipKept const char *from,
                              OwnershipKept const char *to,
                              OwnershipKept const char *amount,
                              OwnershipKept const char *currency,
                              OwnershipKept const char *fee,
                              uint64_t transferIndex,
                              uint64_t blockTimestamp,
                              uint64_t blockNumber,
                              uint64_t blockConfirmations,
                              uint64_t blockTransactionIndex,
                              OwnershipKept const char *blockHash,
                              size_t attributesCount,
                              OwnershipKept const char **attributeKeys,
                              OwnershipKept const char **attributeVals) {
    WKClientTransferBundle bundle = calloc (1, sizeof (struct WKClientTransferBundleRecord));

    // In the case of an error, as indicated by `status`,  we've got no additional information
    // as to the error type.  The transfer/transaction is in the blockchain, presumably it has
    // resulted in a fee paid but no transfer of the desired asset.  A User would be expected to
    // recover in some blockchain specific way - and hopefully can recognize the cause of the error
    // so as to avoid simply creating a transaction to cause it again.

    bundle->status     = status;
    bundle->hash       = strdup (hash);
    bundle->identifier = strdup (identifier);
    bundle->uids       = strdup (uids);
    bundle->from     = strdup (from);
    bundle->to       = strdup (to);
    bundle->amount   = strdup (amount);
    bundle->currency = strdup (currency);
    bundle->fee      = NULL == fee ? NULL : strdup (fee);

    bundle->transferIndex = transferIndex;
    bundle->blockTimestamp = blockTimestamp;
    bundle->blockNumber    = blockNumber;
    bundle->blockConfirmations    = blockConfirmations;
    bundle->blockTransactionIndex = blockTransactionIndex;
    bundle->blockHash = strdup (blockHash);

    // attributes
    bundle->attributesCount = attributesCount;
    bundle->attributeKeys = bundle->attributeVals = NULL;

    if (bundle->attributesCount > 0) {
        bundle->attributeKeys = calloc (bundle->attributesCount, sizeof (char*));
        bundle->attributeVals = calloc (bundle->attributesCount, sizeof (char*));
        for (size_t index = 0; index < bundle->attributesCount; index++) {
            bundle->attributeKeys[index] = strdup (attributeKeys[index]);
            bundle->attributeVals[index] = strdup (attributeVals[index]);
        }
    }

    return bundle;
}

extern void
wkClientTransferBundleRelease (WKClientTransferBundle bundle) {
    //
    free (bundle->uids);
    free (bundle->hash);
    free (bundle->identifier);
    free (bundle->from);
    free (bundle->to);
    free (bundle->amount);
    free (bundle->currency);
    if (NULL != bundle->fee) free (bundle->fee);

    free (bundle->blockHash);

    if (bundle->attributesCount > 0) {
        for (size_t index = 0; index < bundle->attributesCount; index++) {
            free (bundle->attributeKeys[index]);
            free (bundle->attributeVals[index]);
        }
        free (bundle->attributeKeys);
        free (bundle->attributeVals);
    }
    
    memset (bundle, 0, sizeof (struct WKClientTransferBundleRecord));
    free (bundle);
}

static int
wkClientStringCompare (const char *s1, const char *s2) {
    int comparison = strcmp (s1, s2);

    return (comparison > 0
            ? +1
            : (comparison < 0
               ? -1
               :  0));
}

extern int
wkClientTransferBundleCompare (const WKClientTransferBundle b1,
                               const WKClientTransferBundle b2) {
    return (b1->blockNumber < b2->blockNumber
            ? -1
            : (b1->blockNumber > b2->blockNumber
               ? +1
               :  (b1->blockTransactionIndex < b2->blockTransactionIndex
                   ? -1
                   : (b1->blockTransactionIndex > b2->blockTransactionIndex
                      ? +1
                      : (b1->transferIndex < b2->transferIndex
                         ? -1
                         : (b1->transferIndex > b2->transferIndex
                            ? +1
                            :  wkClientStringCompare (b1->uids, b2->uids)))))));
}

extern int
wkClientTransferBundleCompareByBlockheight (const WKClientTransferBundle b1,
                                            const WKClientTransferBundle b2) {
    return (b1->blockNumber < b2-> blockNumber
            ? -1
            : (b1->blockNumber > b2->blockNumber
               ? +1
               :  0));
}

extern WKTransferState
wkClientTransferBundleGetTransferState (const WKClientTransferBundle bundle,
                                        WKFeeBasis confirmedFeeBasis) {
    bool isIncluded = (WK_TRANSFER_STATE_INCLUDED == bundle->status ||    // success
                       (WK_TRANSFER_STATE_ERRORED == bundle->status &&    // error
                        BLOCK_HEIGHT_UNBOUND != bundle->blockNumber &&
                        0 != bundle->blockTimestamp));

    return (isIncluded
            ? wkTransferStateIncludedInit (bundle->blockNumber,
                                           bundle->blockTransactionIndex,
                                           bundle->blockTimestamp,
                                           confirmedFeeBasis,
                                           (WK_TRANSFER_STATE_INCLUDED == bundle->status
                                            ? wkTransferIncludeStatusCreateSuccess ()
                                            : wkTransferIncludeStatusCreateFailure (WK_TRANSFER_INCLUDE_STATUS_FAILURE_UNKNOWN, "via Blockset")))
            : (WK_TRANSFER_STATE_ERRORED == bundle->status
               ? wkTransferStateErroredInit (wkTransferSubmitErrorCreate(WK_TRANSFER_SUBMIT_ERROR_UNKNOWN, "via Blockset"))
               : wkTransferStateInit (bundle->status)));
}

static BRRlpItem
wkClientTransferBundleRlpEncodeAttributes (size_t count,
                                               const char **keys,
                                               const char **vals,
                                               BRRlpCoder coder) {
    BRRlpItem items[count];

    for (size_t index = 0; index < count; index++)
    items[index] = rlpEncodeList2 (coder,
                                   rlpEncodeString (coder, keys[index]),
                                   rlpEncodeString (coder, vals[index]));

    return rlpEncodeListItems (coder, items, count);
}

typedef struct {
    BRArrayOf(char*) keys;
    BRArrayOf(char*) vals;
} WKTransferBundleRlpDecodeAttributesResult;

static WKTransferBundleRlpDecodeAttributesResult
wkClientTransferBundleRlpDecodeAttributes (BRRlpItem item,
                                               BRRlpCoder coder) {
    size_t itemsCount;
    const BRRlpItem *items = rlpDecodeList (coder, item, &itemsCount);

    BRArrayOf(char*) keys;
    array_new (keys, itemsCount);

    BRArrayOf(char*) vals;
    array_new (vals, itemsCount);

    for (size_t index = 0; index < itemsCount; index++) {
        size_t count;
        const BRRlpItem *pair = rlpDecodeList (coder, items[index], &count);
        assert (2 == count);

        array_add (keys, rlpDecodeString (coder, pair[0]));
        array_add (vals, rlpDecodeString (coder, pair[1]));
    }

    return (WKTransferBundleRlpDecodeAttributesResult) { keys, vals };
}

private_extern BRRlpItem
wkClientTransferBundleRlpEncode (WKClientTransferBundle bundle,
                                 BRRlpCoder coder) {
    
    return rlpEncodeList (coder, 16,
                          rlpEncodeUInt64 (coder, bundle->status, 0),
                          rlpEncodeString (coder, bundle->uids),
                          rlpEncodeString (coder, bundle->hash),
                          rlpEncodeString (coder, bundle->identifier),
                          rlpEncodeString (coder, bundle->from),
                          rlpEncodeString (coder, bundle->to),
                          rlpEncodeString (coder, bundle->amount),
                          rlpEncodeString (coder, bundle->currency),
                          rlpEncodeString (coder, bundle->fee),
                          rlpEncodeUInt64 (coder, bundle->blockTimestamp,        0),
                          rlpEncodeUInt64 (coder, bundle->blockNumber,           0),
                          rlpEncodeUInt64 (coder, bundle->blockConfirmations,    0),
                          rlpEncodeUInt64 (coder, bundle->blockTransactionIndex, 0),
                          rlpEncodeString (coder, bundle->blockHash),
                          wkClientTransferBundleRlpEncodeAttributes (bundle->attributesCount,
                                                                     (const char **) bundle->attributeKeys,
                                                                     (const char **) bundle->attributeVals,
                                                                     coder),
                          rlpEncodeUInt64 (coder, bundle->transferIndex,         0));
}

private_extern WKClientTransferBundle
wkClientTransferBundleRlpDecode (BRRlpItem item,
                                 BRRlpCoder coder,
                                 WKFileServiceTransferVersion version) {
    size_t itemsCount;
    const BRRlpItem *items = rlpDecodeList (coder, item, &itemsCount);

    switch (version) {
        case WK_FILE_SERVICE_TYPE_TRANSFER_VERSION_1:
            assert (15 == itemsCount);
            break;
        case WK_FILE_SERVICE_TYPE_TRANSFER_VERSION_2:
            assert (16 == itemsCount);
            break;
        default:
            break;
    }

    char *uids     = rlpDecodeString (coder, items[ 1]);
    char *hash     = rlpDecodeString (coder, items[ 2]);
    char *ident    = rlpDecodeString (coder, items[ 3]);
    char *from     = rlpDecodeString (coder, items[ 4]);
    char *to       = rlpDecodeString (coder, items[ 5]);
    char *amount   = rlpDecodeString (coder, items[ 6]);
    char *currency = rlpDecodeString (coder, items[ 7]);
    char *fee      = rlpDecodeString (coder, items[ 8]);

    uint64_t blockTimestamp        = rlpDecodeUInt64 (coder, items[ 9], 0);
    uint64_t blockNumber           = rlpDecodeUInt64 (coder, items[10], 0);
    uint64_t blockConfirmations    = rlpDecodeUInt64 (coder, items[11], 0);
    uint64_t blockTransactionIndex = rlpDecodeUInt64 (coder, items[12], 0);

    char *blockHash  = rlpDecodeString (coder, items[13]);

    WKTransferBundleRlpDecodeAttributesResult attributesResult =
    wkClientTransferBundleRlpDecodeAttributes (items[14], coder);

    // Set the transferIndex to a default value.
    uint64_t transferIndex = 0;

    switch (version) {
        case WK_FILE_SERVICE_TYPE_TRANSFER_VERSION_1:
            // derive the transferIndex from the UIDS
            if (NULL != uids) {
                char *sepPtr = strrchr (uids, ':');    // "<network>:<hash>:<index>" for Blockset only!

                if (NULL != sepPtr) {
                    char *endPtr = NULL;
                    unsigned long value = strtoul (sepPtr + 1, &endPtr, 10);

                    if ('\0' == endPtr[0])
                        transferIndex = (uint64_t) value;
                }
            }
            break;
        case WK_FILE_SERVICE_TYPE_TRANSFER_VERSION_2:
            transferIndex = rlpDecodeUInt64 (coder, items[15], 0);
            break;
        default:
            break;
    }

    WKClientTransferBundle bundle =
    wkClientTransferBundleCreate ((WKTransferStateType) rlpDecodeUInt64 (coder, items[ 0], 0),
                                  hash,
                                  ident,
                                  uids,
                                  from,
                                  to,
                                  amount,
                                  currency,
                                  (0 == strcmp(fee,"") ? NULL : fee),
                                  transferIndex,
                                  blockTimestamp,
                                  blockNumber,
                                  blockConfirmations,
                                  blockTransactionIndex,
                                  blockHash,
                                  array_count(attributesResult.keys),
                                  (const char **) attributesResult.keys,
                                  (const char **) attributesResult.vals);

    free (blockHash);
    free (fee);
    free (currency);
    free (amount);
    free (to);
    free (from);
    free (ident);
    free (uids);
    free (hash);

    array_free_all (attributesResult.keys, free);
    array_free_all (attributesResult.vals, free);

    return bundle;
}

private_extern size_t
wkClientTransferBundleGetHashValue (WKClientTransferBundle bundle) {
    uint8_t md16[16];
    BRMD5 (md16, bundle->uids, strlen(bundle->uids));
    return *((size_t *) md16);
}

private_extern int
wkClientTransferBundleIsEqual (WKClientTransferBundle bundle1,
                                   WKClientTransferBundle bundle2) {
    return 0 == strcmp (bundle1->uids, bundle2->uids);
}

// MARK: - Transaction Bundle

extern WKClientTransactionBundle
wkClientTransactionBundleCreate (WKTransferStateType status,
                                     OwnershipKept uint8_t *transaction,
                                     size_t transactionLength,
                                     WKTimestamp timestamp,
                                     WKBlockNumber blockHeight) {
    WKClientTransactionBundle bundle = calloc (1, sizeof (struct WKClientTransactionBundleRecord));

    bundle->status = status;

    bundle->serialization = malloc(transactionLength);
    memcpy (bundle->serialization, transaction, transactionLength);
    bundle->serializationCount = transactionLength;

    bundle->timestamp   = timestamp;
    bundle->blockHeight = (WKBlockNumber) blockHeight;

    return bundle;
}

extern void
wkClientTransactionBundleRelease (WKClientTransactionBundle bundle) {
    free (bundle->serialization);
    memset (bundle, 0, sizeof (struct WKClientTransactionBundleRecord));
    free (bundle);
}

extern int
wkClientTransactionBundleCompare (const WKClientTransactionBundle b1,
                                  const WKClientTransactionBundle b2) {
    return wkClientTransactionBundleCompareByBlockheight (b1, b2);
}

extern int
wkClientTransactionBundleCompareByBlockheight (const WKClientTransactionBundle b1,
                                               const WKClientTransactionBundle b2) {
    return (b1->blockHeight < b2-> blockHeight
            ? -1
            : (b1->blockHeight > b2->blockHeight
               ? +1
               :  0));
}

private_extern OwnershipKept uint8_t *
wkClientTransactionBundleGetSerialization (WKClientTransactionBundle bundle,
                                               size_t *serializationCount) {
    *serializationCount = bundle->serializationCount;
    return bundle->serialization;
}

private_extern BRRlpItem
wkClientTransactionBundleRlpEncode (WKClientTransactionBundle bundle,
                                        BRRlpCoder coder) {
    return rlpEncodeList (coder, 4,
                          rlpEncodeUInt64 (coder, bundle->status,      0),
                          rlpEncodeBytes  (coder, bundle->serialization, bundle->serializationCount),
                          rlpEncodeUInt64 (coder, bundle->timestamp,   0),
                          rlpEncodeUInt64 (coder, bundle->blockHeight, 0));
}

private_extern WKClientTransactionBundle
wkClientTransactionBundleRlpDecode (BRRlpItem item,
                                        BRRlpCoder coder) {
    size_t itemsCount;
    const BRRlpItem *items = rlpDecodeList (coder, item, &itemsCount);
    assert (4 == itemsCount);

    BRRlpData serializationData = rlpDecodeBytesSharedDontRelease (coder, items[1]);

    return wkClientTransactionBundleCreate ((WKTransferStateType) rlpDecodeUInt64 (coder, items[0], 0),
                                                serializationData.bytes,
                                                serializationData.bytesCount,
                                                rlpDecodeUInt64(coder, items[2], 0),
                                                rlpDecodeUInt64(coder, items[3], 0));
}

private_extern size_t
wkClientTransactionBundleGetHashValue (WKClientTransactionBundle bundle) {
    uint8_t md16[16];
    BRMD5 (md16, bundle->serialization, bundle->serializationCount);
    return *((size_t *) md16);
}

private_extern int
wkClientTransactionBundleIsEqual (WKClientTransactionBundle bundle1,
                                      WKClientTransactionBundle bundle2) {
    return (bundle1->status             == bundle2->status             &&
            bundle1->timestamp          == bundle2->timestamp          &&
            bundle1->blockHeight        == bundle2->blockHeight        &&
            bundle1->serializationCount == bundle2->serializationCount &&
            0 == memcmp (bundle1->serialization, bundle2->serialization, bundle1->serializationCount));
}

// MARK: - Currency, CurrencyDenomination Bundle

static WKClientCurrencyDenominationBundle
wkClientCurrencyDenominationBundleCreateInternal (OwnershipGiven char *name,
                                                      OwnershipGiven char *code,
                                                      OwnershipGiven char *symbol,
                                                      uint8_t     decimals) {
    WKClientCurrencyDenominationBundle bundle = malloc (sizeof (struct WKCliehtCurrencyDenominationBundleRecord));

    bundle->name = name,
    bundle->code = code;
    bundle->symbol = symbol;
    bundle->decimals = decimals;

    return bundle;
}

extern WKClientCurrencyDenominationBundle
wkClientCurrencyDenominationBundleCreate (const char *name,
                                              const char *code,
                                              const char *symbol,
                                              uint8_t     decimals) {
    return wkClientCurrencyDenominationBundleCreateInternal (strdup (name),
                                                                 strdup (code),
                                                                 strdup (symbol),
                                                                 decimals);
}

static void
wkClientCurrencyDenominationBundleRelease (WKClientCurrencyDenominationBundle bundle) {
    free (bundle->symbol);
    free (bundle->code);
    free (bundle->name);

    memset (bundle, 0, sizeof (struct WKCliehtCurrencyDenominationBundleRecord));
    free (bundle);
}

private_extern BRRlpItem
wkClientCurrencyDenominationBundleRlpEncode (WKClientCurrencyDenominationBundle bundle,
                                                 BRRlpCoder coder) {
    return rlpEncodeList (coder, 4,
                          rlpEncodeString (coder, bundle->name),
                          rlpEncodeString (coder, bundle->code),
                          rlpEncodeString (coder, bundle->symbol),
                          rlpEncodeUInt64 (coder, bundle->decimals, 0));
}

static BRRlpItem
wkClientCurrencyDenominationBundlesRlpEncode (BRArrayOf (WKClientCurrencyDenominationBundle) bundles,
                                                 BRRlpCoder coder) {
    size_t itemsCount = array_count(bundles);
    BRRlpItem items[itemsCount];

    for (size_t index = 0; index < itemsCount; index++)
        items[index] = wkClientCurrencyDenominationBundleRlpEncode (bundles[index], coder);

    return rlpEncodeListItems (coder, items, itemsCount);
}

private_extern WKClientCurrencyDenominationBundle
wkClientCurrencyDenominationBundleRlpDecode (BRRlpItem item,
                                                 BRRlpCoder coder) {
    size_t itemsCount;
    const BRRlpItem *items = rlpDecodeList (coder, item, &itemsCount);
    assert (4 == itemsCount);

    return wkClientCurrencyDenominationBundleCreateInternal (rlpDecodeString (coder, items[0]),
                                                                 rlpDecodeString (coder, items[1]),
                                                                 rlpDecodeString (coder, items[2]),
                                                                 rlpDecodeUInt64 (coder, items[3], 0));
}

static BRArrayOf (WKClientCurrencyDenominationBundle)
wkClientCurrencyDenominationBundlesRlpDecode (BRRlpItem item,
                                                 BRRlpCoder coder) {
    size_t itemsCount;
    const BRRlpItem *items = rlpDecodeList (coder, item, &itemsCount);

    BRArrayOf (WKClientCurrencyDenominationBundle) bundles;
    array_new (bundles, itemsCount);

    for (size_t index = 0; index < itemsCount; index++)
        array_add (bundles, wkClientCurrencyDenominationBundleRlpDecode (items[index], coder));

    return bundles;
}


static WKClientCurrencyBundle
wkClientCurrencyBundleCreateInternal (OwnershipGiven char *id,
                                          OwnershipGiven char *name,
                                          OwnershipGiven char *code,
                                          OwnershipGiven char *type,
                                          OwnershipGiven char *blockchainId,
                                          OwnershipGiven char *address,
                                          bool verified,
                                          OwnershipGiven BRArrayOf(WKClientCurrencyDenominationBundle) denominations) {
    WKClientCurrencyBundle bundle = malloc (sizeof (struct WKClientCurrencyBundleRecord));

    bundle->id = id;
    bundle->name = name;
    bundle->code = code;
    bundle->type = type;
    bundle->bid  = blockchainId;
    bundle->address   = address;
    bundle->verfified = verified;
    bundle->denominations = denominations;

    return bundle;
}

extern WKClientCurrencyBundle
wkClientCurrencyBundleCreate (const char *id,
                                  const char *name,
                                  const char *code,
                                  const char *type,
                                  const char *blockchainId,
                                  const char *address,
                                  bool verified,
                                  size_t denominationsCount,
                                  OwnershipGiven WKClientCurrencyDenominationBundle *denominations) {

    BRArrayOf(WKClientCurrencyDenominationBundle) arrayOfDenominations;
    array_new (arrayOfDenominations, denominationsCount);
    array_add_array (arrayOfDenominations, denominations, denominationsCount);

    return wkClientCurrencyBundleCreateInternal (strdup (id),
                                                     strdup(name),
                                                     strdup(code),
                                                     strdup(type),
                                                     strdup(blockchainId),
                                                     (NULL == address ? NULL : strdup (address)),
                                                     verified,
                                                     arrayOfDenominations);
}

extern void
wkClientCurrencyBundleRelease (WKClientCurrencyBundle bundle) {
    array_free_all (bundle->denominations, wkClientCurrencyDenominationBundleRelease);

    if (bundle->address) free (bundle->address);
    free (bundle->bid);
    free (bundle->type);
    free (bundle->code);
    free (bundle->name);
    free (bundle->id);

    memset (bundle, 0, sizeof (struct WKClientCurrencyBundleRecord));
    free (bundle);
}

static size_t
wkClientCurrencyBundleGetHashValue (WKClientCurrencyBundle bundle) {
    UInt256 identifier;
    BRSHA256(identifier.u8, bundle->id, strlen(bundle->id));
    return (8 == sizeof(size_t)
            ? identifier.u64[0]
            : identifier.u32[0]);
}

// For BRSet
static int
wkClientCurrencyBundleIsEqual (WKClientCurrencyBundle bundle1,
                                    WKClientCurrencyBundle bundle2) {
    return 0 == strcmp (bundle1->id, bundle2->id);
}

extern OwnershipGiven BRSetOf(WKClientCurrencyBundle)
wkClientCurrencyBundleSetCreate (size_t capacity) {
    return BRSetNew ((size_t (*) (const void *)) wkClientCurrencyBundleGetHashValue,
                     (int (*) (const void *, const void *)) wkClientCurrencyBundleIsEqual,
                     capacity);
}

extern void
wkClientCurrencyBundleSetRelease (OwnershipGiven BRSetOf(WKClientCurrencyBundle) bundles) {
    BRSetFreeAll (bundles, (void (*) (void *)) wkClientCurrencyBundleRelease);
}

private_extern BRRlpItem
wkClientCurrencyBundleRlpEncode (WKClientCurrencyBundle bundle,
                                     BRRlpCoder coder) {
    return rlpEncodeList (coder, 8,
                          rlpEncodeString (coder, bundle->id),
                          rlpEncodeString (coder, bundle->name),
                          rlpEncodeString (coder, bundle->code),
                          rlpEncodeString (coder, bundle->type),
                          rlpEncodeString (coder, bundle->bid),
                          rlpEncodeString (coder, bundle->address),
                          rlpEncodeUInt64 (coder, bundle->verfified, 0),
                          wkClientCurrencyDenominationBundlesRlpEncode (bundle->denominations, coder));
}

private_extern WKClientCurrencyBundle
wkClientCurrencyBundleRlpDecode (BRRlpItem item,
                                     BRRlpCoder coder) {
    size_t itemsCount;
    const BRRlpItem *items = rlpDecodeList (coder, item, &itemsCount);
    assert (8 == itemsCount);

    return wkClientCurrencyBundleCreateInternal (rlpDecodeString (coder, items[0]),
                                                     rlpDecodeString (coder, items[1]),
                                                     rlpDecodeString (coder, items[2]),
                                                     rlpDecodeString (coder, items[3]),
                                                     rlpDecodeString (coder, items[4]),
                                                     rlpDecodeString (coder, items[5]),
                                                     rlpDecodeUInt64 (coder, items[6], 0),
                                                     wkClientCurrencyDenominationBundlesRlpDecode (items[7], coder));
}

extern void
wkClientAnnounceCurrenciesSuccess (WKSystem system,
                                   OwnershipGiven WKClientCurrencyBundle *bundles,
                                   size_t bundlesCount) {
    BRArrayOf(WKClientCurrencyBundle) bundlesAsArray;
    array_new (bundlesAsArray, bundlesCount);
    array_add_array(bundlesAsArray, bundles, bundlesCount);

    wkSystemHandleCurrencyBundles (system, bundlesAsArray);
    array_free (bundlesAsArray);
}

extern void
wkClientAnnounceCurrenciesFailure (WKSystem system,
                                   WKClientError error) {
    return;
}
