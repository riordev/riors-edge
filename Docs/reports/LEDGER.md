# LEDGER

The lane's open questions for the design seat, in one place. Answered
questions are deleted; git holds them. Findings and status live in the
session reports, never here.

## The account is per process, and a listen server has one process for two pawns

Riftglass now lives in the account save, and the account is loaded once per
process. In the two-player smoke (O185) both pawns on the listen server
bind their wallet to the server's one account: the second pawn to save
overwrites the balance the first pawn wrote. Solo play is unaffected. Is
the smoke's account one shared pool by design, or does the client's pawn
need its own account carried across the connection? Not built either way;
the crossing site in `LoadGameState` says where the answer goes.
