// Copyright 2011 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "clean.h"

#include <assert.h>
#include <stdio.h>

#include "disk_interface.h"
#include "graph.h"
#include "state.h"
#include "util.h"

Cleaner::Cleaner(State* state,
                 const BuildConfig& config,
                 DiskInterface* disk_interface)
  : state_(state),
    config_(config),
    dyndep_loader_(state, disk_interface),
    removed_(),
    cleaned_(),
    cleaned_files_count_(0),
    disk_interface_(disk_interface),
    status_(0) {
}

int Cleaner::RemoveFile(const string& path) {
  return disk_interface_->RemoveFile(path);
}

bool Cleaner::FileExists(const string& path) {
  string err;
  TimeStamp mtime = disk_interface_->Stat(path, &err);
  if (mtime == -1)
    Error("%s", err.c_str());
  return mtime > 0;  // Treat Stat() errors as "file does not exist".
}

void Cleaner::Report(const string& path) {
  ++cleaned_files_count_;
  if (IsVerbose())
    printf("Remove %s\n", path.c_str());
}

void Cleaner::Remove(GlobalPathStr pathG) {
  string path(pathG.h.data());
  if (!IsAlreadyRemoved(path)) {
    removed_.insert(path);
    if (config_.dry_run) {
      if (FileExists(path))
        Report(path);
    } else {
      int ret = RemoveFile(path);
      if (ret == 0)
        Report(path);
      else if (ret == -1)
        status_ = 1;
    }
  }
}

bool Cleaner::IsAlreadyRemoved(const string& path) {
  set<string>::iterator i = removed_.find(path);
  return (i != removed_.end());
}

void Cleaner::RemoveEdgeFiles(Edge* edge) {
  string depfile = edge->GetUnescapedDepfile();
  if (!depfile.empty())
    Remove(edge->pos_.scope()->GlobalPath(depfile));

  string rspfile = edge->GetUnescapedRspfile();
  if (!rspfile.empty())
    Remove(edge->pos_.scope()->GlobalPath(rspfile));
}

void Cleaner::PrintHeader() {
  if (config_.verbosity == BuildConfig::QUIET)
    return;
  printf("Cleaning...");
  if (IsVerbose())
    printf("\n");
  else
    printf(" ");
  fflush(stdout);
}

void Cleaner::PrintFooter() {
  if (config_.verbosity == BuildConfig::QUIET)
    return;
  printf("%d files.\n", cleaned_files_count_);
}

int Cleaner::CleanAll(bool generator) {
  Reset();
  PrintHeader();
  LoadDyndeps();
  for (vector<Edge*>::iterator e = state_->edges_.begin();
       e != state_->edges_.end(); ++e) {
    // Do not try to remove phony targets
    if ((*e)->is_phony() || (*e)->IsPhonyOutput())
      continue;
    // Do not remove generator's files unless generator specified.
    if (!generator && (*e)->IsGenerator())
      continue;
    for (vector<Node*>::iterator out_node = (*e)->outputs_.begin();
         out_node != (*e)->outputs_.end(); ++out_node) {
      Remove((*out_node)->globalPath());
    }

    RemoveEdgeFiles(*e);
  }
  PrintFooter();
  return status_;
}

void Cleaner::DoCleanTarget(Node* target) {
  if (Edge* e = target->in_edge()) {
    // Do not try to remove phony targets
    if (!e->is_phony() && !e->IsPhonyOutput()) {
      Remove(target->globalPath());
      RemoveEdgeFiles(e);
    }
    for (vector<Node*>::iterator n = e->inputs_.begin(); n != e->inputs_.end();
         ++n) {
      Node* next = *n;
      // call DoCleanTarget recursively if this node has not been visited
      if (cleaned_.count(next) == 0) {
        DoCleanTarget(next);
      }
    }
  }

  // mark this target to be cleaned already
  cleaned_.insert(target);
}

int Cleaner::CleanTarget(Node* target) {
  assert(target);

  Reset();
  PrintHeader();
  LoadDyndeps();
  DoCleanTarget(target);
  PrintFooter();
  return status_;
}

int Cleaner::CleanTargets(int target_count, const char* targets[]) {
  Reset();
  PrintHeader();
  LoadDyndeps();
  for (int i = 0; i < target_count; ++i) {
    string target_name = targets[i];
    uint64_t slash_bits;
    string err;
    if (!CanonicalizePath(&target_name, &slash_bits, &err)) {
      Error("failed to canonicalize '%s': %s", target_name.c_str(), err.c_str());
      status_ = 1;
    } else {
      Node* target = state_->LookupNode(
          state_->root_scope_.GlobalPath(target_name));
      if (target) {
        if (IsVerbose())
          printf("Target %s\n", target_name.c_str());
        DoCleanTarget(target);
      } else {
        Error("unknown target '%s'", target_name.c_str());
        status_ = 1;
      }
    }
  }
  PrintFooter();
  return status_;
}

void Cleaner::DoCleanRule(const Rule* rule) {
  assert(rule);

  for (vector<Edge*>::iterator e = state_->edges_.begin();
       e != state_->edges_.end(); ++e) {
    if ((*e)->rule().name() == rule->name()) {
      if ((*e)->IsPhonyOutput()) {
        continue;
      }
      for (vector<Node*>::iterator out_node = (*e)->outputs_.begin();
           out_node != (*e)->outputs_.end(); ++out_node) {
        Remove((*out_node)->globalPath());
        RemoveEdgeFiles(*e);
      }
    }
  }
}

int Cleaner::CleanRule(const Rule* rule) {
  assert(rule);

  Reset();
  PrintHeader();
  LoadDyndeps();
  DoCleanRule(rule);
  PrintFooter();
  return status_;
}

int Cleaner::CleanRule(const char* rule) {
  assert(rule);

  Reset();
  ScopePosition end_of_root { &state_->root_scope_, kLastDeclIndex };
  const Rule* r = Scope::LookupRuleAtPos(rule, end_of_root);
  if (r) {
    CleanRule(r);
  } else {
    Error("unknown rule '%s'", rule);
    status_ = 1;
  }
  return status_;
}

int Cleaner::CleanRules(int rule_count, char* rules[]) {
  assert(rules);

  Reset();
  PrintHeader();
  LoadDyndeps();
  for (int i = 0; i < rule_count; ++i) {
    const char* rule_name = rules[i];
    ScopePosition end_of_root { &state_->root_scope_, kLastDeclIndex };
    const Rule* rule = Scope::LookupRuleAtPos(rule_name, end_of_root);
    if (rule) {
      if (IsVerbose())
        printf("Rule %s\n", rule_name);
      DoCleanRule(rule);
    } else {
      Error("unknown rule '%s'", rule_name);
      status_ = 1;
    }
  }
  PrintFooter();
  return status_;
}

void Cleaner::Reset() {
  status_ = 0;
  cleaned_files_count_ = 0;
  removed_.clear();
  cleaned_.clear();
}

void Cleaner::LoadDyndeps() {
  // Load dyndep files that exist, before they are cleaned.
  for (vector<Edge*>::iterator e = state_->edges_.begin();
       e != state_->edges_.end(); ++e) {
    if (Node* dyndep = (*e)->dyndep_) {
      // Capture and ignore errors loading the dyndep file.
      // We clean as much of the graph as we know.
      std::string err;
      dyndep_loader_.LoadDyndeps(dyndep, &err);
    }
  }
}
