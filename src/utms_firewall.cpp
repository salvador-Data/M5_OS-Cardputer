#include "utms_firewall.h"

#include "m5os_vfs.h"
#include "utms_threat_pack.h"

#include <ArduinoJson.h>
#include <SD.h>

namespace m5os::utms {

namespace {

bool urlMatchesPattern(const String& url, const String& pattern) {
    if (!pattern.length()) return false;
    String u = url;
    String p = pattern;
    u.toLowerCase();
    p.toLowerCase();
    return u.indexOf(p) >= 0;
}

}  // namespace

std::vector<FirewallRule> loadFirewallRules() {
    std::vector<FirewallRule> rules;
    if (!ensureUtmsDirs()) return rules;
    if (!SD.exists(vfs::kFirewallRulesPath)) return rules;

    File in = SD.open(vfs::kFirewallRulesPath, FILE_READ);
    if (!in) return rules;
    JsonDocument doc;
    deserializeJson(doc, in);
    in.close();

    JsonArray arr = doc["rules"].as<JsonArray>();
    if (arr.isNull()) {
        JsonArray allow = doc["allow"].as<JsonArray>();
        JsonArray deny = doc["deny"].as<JsonArray>();
        if (!allow.isNull()) {
            for (JsonVariant v : allow) {
                FirewallRule r;
                r.action = "allow";
                r.pattern = v.as<String>();
                r.id = "a" + String(rules.size());
                r.enabled = true;
                if (r.pattern.length()) rules.push_back(r);
            }
        }
        if (!deny.isNull()) {
            for (JsonVariant v : deny) {
                FirewallRule r;
                r.action = "deny";
                r.pattern = v.as<String>();
                r.id = "d" + String(rules.size());
                r.enabled = true;
                if (r.pattern.length()) rules.push_back(r);
            }
        }
        return rules;
    }

    for (JsonObject row : arr) {
        FirewallRule r;
        r.id = row["id"] | String(rules.size());
        r.action = row["action"] | "deny";
        r.pattern = row["pattern"] | "";
        r.enabled = row["enabled"] | true;
        r.action.toLowerCase();
        if (r.pattern.length()) rules.push_back(r);
    }
    return rules;
}

bool saveFirewallRules(const std::vector<FirewallRule>& rules) {
    if (!ensureUtmsDirs()) return false;
    JsonDocument doc;
    JsonArray arr = doc["rules"].to<JsonArray>();
    for (const auto& r : rules) {
        JsonObject row = arr.add<JsonObject>();
        row["id"] = r.id;
        row["action"] = r.action;
        row["pattern"] = r.pattern;
        row["enabled"] = r.enabled;
    }
    String body;
    serializeJson(doc, body);
    const String tmp = String(vfs::kFirewallRulesPath) + ".tmp";
    File out = SD.open(tmp.c_str(), FILE_WRITE);
    if (!out) return false;
    const size_t written = out.print(body);
    out.close();
    if (written != body.length()) {
        SD.remove(tmp.c_str());
        return false;
    }
    if (SD.exists(vfs::kFirewallRulesPath)) SD.remove(vfs::kFirewallRulesPath);
    if (!SD.rename(tmp.c_str(), vfs::kFirewallRulesPath)) return false;
    appendLog("fw_save", String(rules.size()) + " rules");
    return true;
}

bool urlAllowedByFirewall(const String& url) {
    if (!vfs::isMounted()) return true;
    const std::vector<FirewallRule> rules = loadFirewallRules();
    if (rules.empty()) return true;

    for (const auto& r : rules) {
        if (!r.enabled || r.action != "allow") continue;
        if (urlMatchesPattern(url, r.pattern)) return true;
    }
    for (const auto& r : rules) {
        if (!r.enabled || r.action != "deny") continue;
        if (urlMatchesPattern(url, r.pattern)) {
            logFirewallBlock(url, r.id);
            return false;
        }
    }
    return true;
}

void logFirewallBlock(const String& url, const String& ruleId) {
    appendLog("fw_block", ruleId + " " + url);
}

}  // namespace m5os::utms
