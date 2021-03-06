#include "CommandBase.h"

#include <utility>
#include <algorithm>
#include <chrono>
#include <NumericConstants.h>
#include "complyer/util/FormulaStringUtil.hpp"

CommandBase::CommandBase(const ModelSpecification& m) {
  verifier = new Verifiers(m);
  command = new Commands(verifier, m);
}

CommandResponse CommandBase::perform(const std::string &line) {
  for(auto & cur_command : command->commands){
    if(cur_command->parse(line)){
      cur_command->prePerform();
      CommandResponse res = cur_command->perform();
      cur_command->postPerform();
      return res;
    }
  }
  return CommandResponse(std::string(), int(), "Invalid Command", long());
}

bool CommandBase::CommandInterface::parse(const std::string &line) {
  std::stringstream stream(line);
  std::string current_command;
  stream >> current_command;
  std::transform(current_command.begin(), current_command.end(), current_command.begin(), ::toupper);
  return current_command == this->getOperation();
}

CommandBase::CommandInterface::CommandInterface(std::string op, CommandBase::Commands *commands,
                                                CommandBase::Verifiers *verifiers,
                                                ModelSpecification modelSpecification) : operation(std::move(op)) {
  this->command = commands;
  this->verifier = verifiers;
  this->model_specification = std::move(modelSpecification);
}

CommandResponse CommandBase::CommandQuit::perform() {
  exit(0);
}

std::string CommandBase::CommandQuit::help() {
  std::string help_string;
  help_string = "usage: quit\n";
  help_string += "Ends current run of complyer\n";
  return help_string;
}

CommandResponse CommandBase::CommandTrace::perform() {
  if(common_verifier == nullptr) {
    throw std::logic_error("verifier cannot be null for trace");
  }
  try {
    auto start = std::chrono::high_resolution_clock::now();
    std::stringstream stream;
    auto trace = common_verifier->getTrace();
    auto states = trace.getStates();
    auto symbols = trace.getSymbols();
    for(int j=0; j<states.size();j++) {
      auto state = states[j];
      int vars = symbols.size();
      stream << "State " << j+1 << '\n';
      for(int i = 0; i < vars; ++i) {
        stream << "\t" << symbols[i] << " = " << state[i] << "\n";
      }
      stream << '\n';
    }
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
    return CommandResponse(this->getOperation(), 1, stream.str(), duration.count());
  } catch (std::exception &e) {
    return CommandResponse(this->getOperation(), 0, e.what(), 0);
  }
}

std::string CommandBase::CommandTrace::help() {
  std::string help_string;
  help_string = "usage: trace\n";
  help_string += "Returns a sequence of states not satisfying last property. Fails if last property was SAT\n";
  return help_string;
}

CommandResponse CommandBase::CommandLength::perform() {
  if(common_verifier == nullptr) {
    throw std::logic_error("verifier cannot be null for length");
  } 
  auto start = std::chrono::high_resolution_clock::now();
  int result = common_verifier->getLength();
  auto stop = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
  return CommandResponse(this->getOperation(), result, std::to_string(result), duration.count());
}

std::string CommandBase::CommandLength::help() {
  std::string help_string;
  help_string = "usage: length\n";
  help_string += "Returns diameter of state-space checked by last property.\n";
  return help_string;
}

bool CommandBase::CommandSafetyalgorithm::parse(const std::string &line) {
  if(!CommandInterface::parse(line)) return false;
  std::stringstream stream(line);
  std::string cmd; stream >> cmd;
  stream >> (this->name);
  return true;
}

CommandResponse CommandBase::CommandSafetyalgorithm::perform() {
  auto start = std::chrono::high_resolution_clock::now();
  std::transform(name.begin(), name.end(), name.begin(), ::toupper);
  int result = 0;
  if(name == "KINDUCTION") { command->safetyspec->setVerifier(verifier->k_induction_verifier); }
  else if(name == "IC3") { command->safetyspec->setVerifier(verifier->ic3_verifier); }
  else if(name == "PNET") { command->safetyspec->setVerifier(verifier->pnet_coverability); }
  else { result = 1; }
  auto stop = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
  std::string message = (result == 0) ? "Algorithm set to " + name : "Algorithm not found";
  return CommandResponse(this->getOperation(), result, message, duration.count());
}

std::string CommandBase::CommandSafetyalgorithm::help() {
  std::string help_string;
  help_string = "usage: safetyalgorithm <ALGORITHM>\n";
  help_string += "Changes the algorithm used for safety verification.\n";
  return help_string;
}


bool CommandBase::CommandSafetyspec::parse(const std::string &line) {
  if(!CommandInterface::parse(line)) return false;
  std::stringstream stream(line);
  std::string cmd; stream >> cmd;
  std::getline(stream, this->property);
  return true;
}

CommandResponse CommandBase::CommandSafetyspec::perform() {
  auto start = std::chrono::high_resolution_clock::now();
  auto label_mapper = this->model_specification.getLabelMapper();
  this->property = substitute(this->property, label_mapper);
  int result = this->common_verifier->check(property)?NumericConstants::SAT : NumericConstants::UNSAT;
  auto stop = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
  std::string result_string = result == NumericConstants::SAT ? "SAT" : "UNSAT";
  return CommandResponse(getOperation(), result, result_string, duration.count());
}

void CommandBase::CommandSafetyspec::prePerform() {
  if(this->common_verifier == nullptr) {
    if((this->model_specification).isConcurrent()) {
      this->common_verifier = (this->verifier)->pnet_coverability;
    } else {
      this->common_verifier = (this->verifier)->k_induction_verifier;
    }
  }
  command->trace->setVerifier(common_verifier);
  command->length->setVerifier(common_verifier);
}

std::string CommandBase::CommandSafetyspec::help() {
  std::string help_string;
  help_string = "usage: safetyspec <SAFETYPROPERTY>\n";
  help_string += "Verifies the given safety property against the system using k-Induction.\n";
  return help_string;
}

bool CommandBase::CommandBound::parse(const std::string &line) {
  if(!CommandInterface::parse(line)) return false;
  std::stringstream stream(line);
  std::string cmd; stream >> cmd;
  stream >> bound;
  return true;
}

CommandResponse CommandBase::CommandBound::perform() {
  auto start = std::chrono::high_resolution_clock::now();
  this->verifier->ltl_bmc_verifier->setBound(bound);
  auto stop = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
  return CommandResponse(getOperation(), bound, "Set " + std::to_string(bound), duration.count());
}

std::string CommandBase::CommandBound::help() {
  std::string help_string;
  help_string = "usage: bound <INTEGER>\n";
  help_string += "Sets the bound for verification of ltl properties.\n";
  return help_string;
}

bool CommandBase::CommandLtlspec::parse(const std::string &line) {
  if(!CommandInterface::parse(line)) return false;
  std::stringstream stream(line);
  std::string cmd; stream >> cmd;
  std::getline(stream, property);
  return true;
}

CommandResponse CommandBase::CommandLtlspec::perform() {
  auto start = std::chrono::high_resolution_clock::now();
  auto label_mapper = this->model_specification.getLabelMapper();
  this->property = substitute(this->property, label_mapper);
  int result = this->verifier->ltl_bmc_verifier->check(property)?NumericConstants::SAT : NumericConstants::UNSAT;
  auto stop = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
  std::string result_string = result == NumericConstants::SAT ? "SAT" : "UNSAT";
  return CommandResponse(getOperation(), result, result_string, duration.count());
}

void CommandBase::CommandLtlspec::prePerform() {
  command->trace->setVerifier(verifier->ltl_bmc_verifier);
  command->length->setVerifier(verifier->ltl_bmc_verifier);
}

std::string CommandBase::CommandLtlspec::help() {
  std::string help_string;
  help_string = "usage: ltlspec <LTLPROPERTY>\n";
  help_string += "Verifies the given ltl property against the system using ltl bmc.\n";
  return help_string;
}

CommandBase::Commands::Commands(Verifiers *verifiers, const ModelSpecification& ms) {
  commands[QUIT] = quit = new CommandQuit(this, verifiers, ms);
  commands[HELP] = help = new CommandHelp(this, verifiers, ms);
  commands[TRACE] = trace = new CommandTrace(this, verifiers, ms);
  commands[BOUND] = bound = new CommandBound(this, verifiers, ms);
  commands[LENGTH] = length = new CommandLength(this, verifiers, ms);
  commands[LTLSPEC] = ltlspec = new CommandLtlspec(this, verifiers, ms);
  commands[SAFETYSPEC] = safetyspec = new CommandSafetyspec(this, verifiers, ms);
  commands[SAFETY_ALGORITHM] = safety_algorithm = new CommandSafetyalgorithm(this, verifiers, ms); 
}

CommandBase::Verifiers::Verifiers(ModelSpecification ms) {
  verifiers[LTL_BMC] = ltl_bmc_verifier = new ltlBmc(ms.getSymbols(), ms.getKripke().getI(), ms.getKripke().getT());
  verifiers[K_INDUCTION] = k_induction_verifier = new kInduction(ms.getSymbols(), ms.getKripke().getI(), ms.getKripke().getT());
  verifiers[IC_3] = ic3_verifier = new IC3(ms.getSymbols(), ms.getKripke().getI(), ms.getKripke().getT());
  if(ms.isConcurrent()) { verifiers[P_NET] = pnet_coverability = new PnetCoverability(ms.getSymbols(), ms.getPetriNet().getInitialMarking(), ms.getPetriNet().getIncidenceMatrix()); }
  else { verifiers[P_NET] = pnet_coverability = nullptr; }
}

bool CommandBase::CommandHelp::parse(const std::string &line) {
  if(!CommandInterface::parse(line)) return false;
  std::stringstream stream(line);
  std::string cmd; stream >> cmd;
  if(stream >> cmd_help){
    std::transform(cmd_help.begin(), cmd_help.end(), cmd_help.begin(), ::toupper);
  }
  else {cmd_help = "";}
  return true;
}

CommandResponse CommandBase::CommandHelp::perform() {
  auto start = std::chrono::high_resolution_clock::now();
  std::string help_string;
  if(cmd_help.empty()) help_string = general_help();
  else help_string = specific_help();
  auto stop = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
  return CommandResponse(getOperation(), 0, help_string, duration.count());
}

std::string CommandBase::CommandHelp::help() {
  std::string help_string;
  help_string = "usage: help [COMMAND]\n";
  help_string += "Gives help for each command\n";
  return help_string;
}

std::string CommandBase::CommandHelp::general_help() {
  std::string help_string;
  for(auto & cur_command : command->commands){
    help_string += cur_command->help();
    help_string += "\n";
  }
  return help_string;
}

std::string CommandBase::CommandHelp::specific_help() {
  for(auto & cur_command : command->commands){
    if(cur_command->getOperation() == cmd_help)
      return cur_command->help();
  }
  std::string none_matched = "Invalid Command. Please run 'help' without arguments to get list of commands";
  return none_matched;
}
