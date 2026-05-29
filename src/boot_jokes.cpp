#include "boot_jokes.h"

#include <esp_random.h>
#include <string.h>

namespace m5os::boot {

namespace {

// PG-13 cybersec / hacker / relationship humor — authorized lab framing.
constexpr const char* kBootJokes[] = {
    "My firewall and I: exclusive, no inbound.",
    "She wanted commitment; I gave her a signed cert.",
    "Trust but verify — I GPG-signed the prenup.",
    "Our love runs TLS 1.3: forward secrecy.",
    "Date night: patching CVEs together.",
    "He ghosted me; I just revoked his SSH key.",
    "Roses are red, violets are blue, 443 is open, but not for you.",
    "She said share feelings; I shared a read-only folder.",
    "My ex was a brute-force attack on my heart.",
    "Couples therapy: rotating our shared API keys.",
    "I fell for a phishing email — and then her.",
    "Two-factor us: something you know, something you are.",
    "Love is blind; my IDS is not.",
    "We met on a VLAN; kept it segmented.",
    "He said forever; TTL said otherwise.",
    "My partner audits my logs — healthy relationship.",
    "Swipe right only after certificate pinning.",
    "Heartbreak is just a failed handshake.",
    "She wanted space; I gave her an air-gapped VM.",
    "Our song: the dial-up modem screech.",
    "Jealousy is unauthorized lateral movement.",
    "I proposed with a diamond; she wanted a YubiKey.",
    "Breakup protocol: revoke, rotate, rebuild.",
    "He said he had nothing to hide; zero logs found.",
    "We bonded over a mutual hatred of plaintext.",
    "Long distance? More like long subnet mask.",
    "My love language is timely security patches.",
    "She left me on read; I left her on block list.",
    "Anniversary gift: a renewed Let's Encrypt cert.",
    "Trust falls fail; trust chains don't.",
    "He said I was too clingy; I said I was sticky sessions.",
    "Our first kiss: mutual TLS, both verified.",
    "Dating apps are just port scanners for souls.",
    "She wanted the keys to my heart; I gave least privilege.",
    "We argue about tabs vs spaces, not infidelity.",
    "My heart has rate limiting after the last ex.",
    "He promised uptime; delivered only downtime.",
    "Couple goals: shared threat model, separate sudo.",
    "I wrote her a poem in Base64; she decoded feelings.",
    "Love at first ping, heartbreak at packet loss.",
    "She said talk dirty; I explained buffer overflows.",
    "Our meet-cute: collided in the same /24.",
    "He forgot our anniversary; I forgot his password reset.",
    "Relationship status: it's complicated, like DNS.",
    "I gave her my heart; she ran it through VirusTotal.",
    "Two hearts, one subnet, zero trust.",
    "My ex still has root; working on privilege escalation.",
    "She wanted fireworks; I showed her a honeypot alert.",
    "We communicate via encrypted Signal, not hints.",
    "He said trust me; I asked for his audit trail.",
    "Love is patient; my SIEM is impatient.",
    "Our vows: confidentiality, integrity, availability.",
    "She said open up; I opened port 443 only.",
    "Heart emoji rejected; sent signed ASCII instead.",
    "We split the bill and the /etc/passwd file.",
    "My type? Someone who reads the RFC before cuddling.",
    "He said he changed; git log says otherwise.",
    "Anniversary dinner: table for two, VLAN for two.",
    "I love you 3000; she loves me 401 Unauthorized.",
    "Couples who pentest together, stay together.",
    "She asked for my password; I offered a hardware token.",
    "Our love story: authorized penetration test.",
};

constexpr size_t kBootJokeCount = sizeof(kBootJokes) / sizeof(kBootJokes[0]);

const char* gCachedJoke = nullptr;

void copyTrunc(char* dst, const char* src, size_t maxLen) {
    if (!dst || maxLen == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, maxLen - 1);
    dst[maxLen - 1] = '\0';
}

bool isSpace(char c) { return c == ' ' || c == '\t'; }

}  // namespace

size_t bootJokeCount() { return kBootJokeCount; }

const char* randomJokeForBoot() {
    if (gCachedJoke) return gCachedJoke;
    const uint32_t idx = esp_random() % static_cast<uint32_t>(kBootJokeCount);
    gCachedJoke = kBootJokes[idx];
    return gCachedJoke;
}

void wrapBootJoke(const char* joke, char* line1, char* line2, size_t maxChars) {
    if (!line1 || !line2 || maxChars < 8) return;
    line1[0] = '\0';
    line2[0] = '\0';
    if (!joke || joke[0] == '\0') return;

    const size_t len = strlen(joke);
    if (len <= maxChars) {
        copyTrunc(line1, joke, maxChars + 1);
        return;
    }

    size_t split = maxChars;
    while (split > 0 && !isSpace(joke[split])) --split;
    if (split == 0) split = maxChars;

    copyTrunc(line1, joke, split + 1);
    while (joke[split] != '\0' && isSpace(joke[split])) ++split;
    copyTrunc(line2, joke + split, maxChars + 1);
}

}  // namespace m5os::boot
