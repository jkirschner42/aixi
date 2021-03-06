#include "search.hpp"

#include "agent.hpp"
#include "util.hpp"

#include <cmath>

SearchNode::SearchNode(bool is_chance_node,
    unsigned int num_actions)
    : m_chance_node(is_chance_node), m_visits(0) {
    
    //make list of unexplored actions
    if (!is_chance_node) {
        for (unsigned int i = 0; i < num_actions; ++i) {
            m_unexplored_actions.push_back(i);
        }
    }
}

SearchNode::~SearchNode() {
    // Destroy any allocated children.
    for (std::map<unsigned int, SearchNode*>::iterator it = m_child.begin();
    		it != m_child.end(); ++it) {
        if (it->second != NULL) {
            delete it->second;
        }
    }
}

// simulate a sequence of random actions, returning the accumulated reward.
static reward_t playout(Agent &agent, unsigned int playout_len) {
	reward_t r = 0;
	for (unsigned int i = 0; i < playout_len; ++i) {
	    // Pick a random action
	    action_t a = agent.genRandomAction();
	    agent.modelUpdate(a);
		
		// Generate a random percept distributed according to the agent's
		// internal model of the environment.
        percept_t rew;
        percept_t obs;
	    agent.genPerceptAndUpdate(obs, rew);
	    
	    r = r + rew;
    }
	return r;
}

// UCB action selection strategy
action_t SearchNode::selectAction(Agent& agent, unsigned int dfr) {
    //choose unexplored action at random if any and append to tree
    if (!m_unexplored_actions.empty()) {
        int action_index = randRange(m_unexplored_actions.size());
        action_t action = m_unexplored_actions.at(action_index);
        m_unexplored_actions.erase(m_unexplored_actions.begin() + action_index);

        m_child[action] = new SearchNode(true, agent.numActions());
        return action;
    } else {
        action_t arg_max = 0;
        double C = sqrt(2);

        double max = (1.0 / (dfr * agent.maxReward())) * m_child[0]->m_mean
                    + C * sqrt((log(m_visits)/m_child[0]->m_visits));
        //search for argmax
        for (action_t a = 1; a < agent.numActions(); ++a) {
            double f = (1.0 / (dfr * agent.maxReward())) * m_child[a]->m_mean
                    + C * sqrt((log(m_visits)/m_child[a]->m_visits));
            // Notes: agent.minReward() defined as 0, so omitted.
            if (f > max) {
                max = f;
                arg_max = a;
            }
        }
        
        return arg_max;
    }
}

// Sample one possible sequence of future events, up to 'dfr' cycles.
reward_t SearchNode::sample(Agent &agent, unsigned int dfr) {
    double newReward;
    if (dfr == 0) {
        return 0;
    } else if (m_chance_node) {
        // Generate whole observation-reward percept,
        // according to the agent's model of the environment.
        percept_t obs;
        percept_t rew;
        agent.genPerceptAndUpdate(obs, rew);

        // Calculate the index of whole percept
        percept_t percept = (rew << agent.numObsBits()) | obs;
        
        if (m_child.count(percept) == 0) {
            m_child[percept] = new SearchNode(false, agent.numActions());
        }
        newReward = rew + m_child[percept]->sample(agent, dfr - 1);
    } else if (m_visits == 0) {
        newReward = playout(agent, dfr);
    } else {
    	// Select an action to sample.
        action_t action = selectAction(agent, dfr);
        agent.modelUpdate(action);
        newReward = m_child[action]->sample(agent, dfr);
    }
    // Update our estimate of the future reward.
    m_mean = (1.0 / (double) (m_visits + 1)) * (newReward + m_visits * m_mean);
    ++m_visits;
    return newReward;
}

// determine the best action by searching ahead using MCTS
extern action_t search(Agent &agent, timelimit_t timelimit) {

    //save agent's state
    ModelUndo undo = ModelUndo(agent);

    SearchNode search_tree(false, agent.numActions());

    //sample
    for(visits_t i = 0; i < timelimit; ++i){    
        search_tree.sample(agent, agent.horizon());
        agent.modelRevert(undo);
    }
    
    // Choose the action that has the highest expected reward.
    // We assume timelimit is large enough so that every action was sampled.
    // This is asserted in main::mainLoop().
    double best_reward = search_tree.child(0)->expectation();
    unsigned int best_action = 0;
    for (unsigned int i = 1; i < agent.numActions(); ++i) {
        if (search_tree.child(i)->expectation() > best_reward) {
            best_reward = search_tree.child(i)->expectation();
            best_action = i;
        }
    }
    
    return best_action;
}

