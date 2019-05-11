# BOS escrow

An escrow contract designed for paying worker proposals.  The intention is that the authority for this account would be nulled.

> Original source code was gratefully authored by [eosDAC](https://github.com/eosdac/dacescrow).

## Caveats
- The sender of an escrow will temporarily be whitelisted to BOS executives. In the future anyone may be a sender
- The sender may only have one unfilled escrow at any given time, however they may have many filled escrows
- To fill an escrow the sender must transfer the `BOS` tokens to this contract. An unfilled escrow will be filled
- The receiver is considered as always approving the escrow. An approval must come from either the sender or the approver
- The sender may only cancel an escrow that has not been filled
- The sender may only refund an escrow that has passed it's expiry
- Unapprove only removes an existing approval, if the action is made before the receiver uses the claim action
- A sender may extend the expiry but not shorten it
- The approver may change extend or shorten the expiry
- The approver may close an escrow. This is essentially the same as refunding it, however without waiting for the expiry to lapse
- The approver may Lock and Unlock an escrow. This prevents ALL actions except unlock and actions made by the approver.

<h1 class="contract">
    init
</h1>

## ACTION: `init`

**PARAMETERS:**

- __sender__ is an eosio account name.
- __receiver__ is an eosio account name.
- __approver__ is an eosio account name.
- __expires__ The date/time after which the escrow amount can be refunded by the sender.
- __memo__ is a memo to send as the eventual transfer memo at the end of the escrow contract.
- __ext_reference__ is a reference to to external id held my another contract or entity as opposed to the internal auto-incrementing key.

**INTENT:** The intent of init is to create an empty escrow payment agreement for safe and secure funds transfer protecting both sender and receiver for a determined amount of time.

> **Warning**: This action will store the content on the chain in the history logs and the data cannot be deleted later so therefore should only store a unidentifiable hash of content rather than human readable content.

<h1 class="contract">
    transfer
</h1>

## ACTION: `transfer`

**PARAMETERS:**

- __from__ is an eosio account name.
- __to__ is an eosio account name.
- __quantity__ is an eosio asset name.
- __memo__ is a string that provides a memo for the transfer action.

**INTENT:** The intent of transfer is to listen and react to the eosio.token contract's transfer action and ensure the correct parameters have been included in the transfer action.

> **Warning**: This action will store the content on the chain in the history logs and the data cannot be deleted later.

<h1 class="contract">
    approve
</h1>

## ACTION: `approve`

**PARAMETERS:**

- __key__ is a unique identifying integer for an escrow entry.
- __approver__ is an eosio account name. (BET Account)

**INTENT:** The intent of approve is to approve the release of funds to the intended receiver. Each escrow agreement requires at least 7 BET permissions (excluding sender) to grant fund release.

> **Warning**: This action will store the content on the chain in the history logs and the data cannot be deleted later.

<h1 class="contract">
approveext
</h1>

## ACTION: `approveext`

**PARAMETERS:**

- __ext_key__ is a unique identifying integer for an escrow entry as supplied by an external key source.
- __approver__ is an eosio account name. (BET Account)

**INTENT:** The intent of approve is to approve the release of funds to the intended receiver. Each escrow agreement requires at least 7 BET permissions (excluding sender) to grant fund release.

> **Warning**: This action will store the content on the chain in the history logs and the data cannot be deleted later.

<h1 class="contract">
    unapprove
</h1>

## ACTION: `unapprove`

**PARAMETERS:**

- __key__ is a unique identifying integer for an escrow entry.
- __disapprover__ is an eosio account name.

**INTENT:** The intent of unapprove is to unapprove the release of funds to the intended receiver from a previous approved action.

> **Warning**: This action will store the content on the chain in the history logs and the data cannot be deleted later.

<h1 class="contract">
    unapproveext
</h1>

## ACTION: `unapproveext`

**PARAMETERS:**

- __ext_key__ is a unique identifying integer for an escrow entry as supplied by an external key source.
- __disapprover__ is an eosio account name.

**INTENT:** The intent of unapprove is to unapprove the release of funds to the intended receiver from a previous approved action.

> **Warning**: This action will store the content on the chain in the history logs and the data cannot be deleted later.

<h1 class="contract">
    claim
</h1>

## ACTION: `claim`

**PARAMETERS:**

- __key__ is a unique identifying integer for an escrow entry.

**INTENT:** The intent of claim is to claim the escrowed funds for an intended receiver after an escrow agreement has met the required approvals.

**TERM:** This action lasts for the duration of the time taken to process the transaction.

<h1 class="contract">
    claimext
</h1>

## ACTION: `claimext`

**PARAMETERS:**

- __ext_key__ is a unique identifying integer for an escrow entry as supplied by an external key source.

**INTENT:** The intent of claim is to claim the escrowed funds for an intended receiver after an escrow agreement has met the required approvals.

**TERM:** This action lasts for the duration of the time taken to process the transaction.

<h1 class="contract">
    refund
</h1>

## ACTION: `refund`

**PARAMETERS:**
- __key__ is a unique identifying integer for an escrow entry.

**INTENT:** The intent of refund is to return the escrowed funds back to the original sender. This action can only be run after the contract has met the intended expiry time.

**TERM:** This action lasts for the duration of the time taken to process the transaction.

<h1 class="contract">
  refundext
</h1>

## ACTION: `refundext`

**PARAMETERS:**

- __ext_key__ is a unique identifying integer for an escrow entry as supplied by an external key source.

**INTENT:** The intent of refund is to return the escrowed funds back to the original sender. This action can only be run after the contract has met the intended expiry time.

**TERM:** This action lasts for the duration of the time taken to process the transaction.

<h1 class="contract">
  cancel
</h1>

## ACTION: `cancel`

**PARAMETERS:**

- __key__ is a unique identifying integer for an escrow entry.

**INTENT:** The intent of cancel is to cancel an escrow agreement. This action can only be performed by the sender as long as no funds have already been transferred for the escrow agreement. Otherwise they would need to wait for the expiry time and then use the refund action.

**TERM:** This action lasts for the duration of the time taken to process the transaction.

<h1 class="contract">
  cancelext
</h1>

## ACTION: `cancelext`

**PARAMETERS:**

- __ext_key__ is a unique identifying integer for an escrow entry as supplied by an external key source.

**INTENT:** The intent of cancel is to cancel an escrow agreement. This action can only be performed by the sender as long as no funds have already been transferred for the escrow agreement. Otherwise they would need to wait for the expiry time and then use the refund action.

**TERM:** This action lasts for the duration of the time taken to process the transaction.

<h1 class="contract">
  clean
</h1>

## ACTION: `clean`

**INTENT:** The intent of clean is remove all existing escrow agreements for developer purposes. This can only be run with _self permission of the contract which would be unavailable on the main net once the contract permissions are removed for the contract account.

**TERM:** This action lasts for the duration of the time taken to process the transaction.
