# env should include APIKEY as variable
source .env
SCOREBASEURL="https://gameinfo.daraempire.com/api/v001/khor-services/GameTracker/SendScore"
NEWGAMEBASEURL="https://gameinfo.daraempire.com/api/v001/khor-services/GameTracker/NewGame"

SendScore() {
    local CHARNAME="$1"
    local SCORE=$2
    local GAMEID="$3"

    response=$(curl -sS -X POST\
        -H "Content-Type: application/json" \
        -H "X-Api-Key: $APIKEY" \
        -d "{\"score\":$SCORE,\"avatarId\":\"A1\",\"charName\":\"$CHARNAME\"}" \
        "$SCOREBASEURL")

    echo "$response" | jq -r '
        if .leaderboard | length > 0 then
            "Leader: \(.leaderboard[0].charName) (\(.leaderboard[0].totalScore)) | Cooldown: \(.cooldownActive) | Next in: \(.nextGameStartsInSeconds)s"
        else
            "No leader | Cooldown: \(.cooldownActive) | Next in: \(.nextGameStartsInSeconds)s"
        end
    '
}



SendScore Bernie 120 
SendScore Joinix 110 
SendScore Hatari 100 
SendScore Trulan 90  
SendScore Nanasa 80  
SendScore Gursyn 70  
SendScore Bollie 60  
SendScore Jajabi 40  
SendScore PifPaf 30  
