#pragma once

#include <Arduino.h>
#include <vector>

namespace m5os::utms {

struct FirewallRule {
    String id;
    String action;
    String pattern;
    bool enabled = true;
};

std::vector<FirewallRule> loadFirewallRules();
bool saveFirewallRules(const std::vector<FirewallRule>& rules);
bool urlAllowedByFirewall(const String& url);
void logFirewallBlock(const String& url, const String& ruleId);

}  // namespace m5os::utms
