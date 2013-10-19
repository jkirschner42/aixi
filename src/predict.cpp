#include "predict.hpp"

#include <cassert>
#include <cmath>
#include <errno.h>
#include "util.hpp"
#include <iostream>

// compute log(0.5)
static const double log_half = log(0.5);

CTNode::CTNode(void) :
    m_log_prob_est(0.0),
    m_log_prob_weighted(0.0){

    m_count[0] = 0;
    m_count[1] = 0;
    m_child[0] = NULL;
    m_child[1] = NULL;
}


CTNode::~CTNode(void) {
    if (m_child[0]) delete m_child[0];
    if (m_child[1]) delete m_child[1];
}


// number of descendants of a node in the context tree
size_t CTNode::size(void) const {

    size_t rval = 1;
    rval += child(false) ? child(false)->size() : 0;
    rval += child(true)  ? child(true)->size()  : 0;
    return rval;
}


// compute the logarithm of the KT-estimator update multiplier
double CTNode::logKTMul(symbol_t sym) const {
    return log( (double) (m_count[sym] + 0.5)
                / (double) (m_count[false] + m_count[true] + 1) );
    
    //TODO probably needs <cmath> for log.
    // Note, CTNode includes a visits() function, which we could use in place
    // of m_count[false] + m_count[true]. That's optional (they do exactly the
    // same thing, just visits() looks shorter).

    /* We could consider using a cache (like the table in the lectures) where
       we calculate a bunch of smallish KT estimates, which are looked up
       rather than re-computed every time we call this function.
       I imagine a lookup will be faster than using log(), and considering
       we'll be constantly updating the CTW tree, this should improve speed a
       little. At least in the beginning, before we get really large numbers
       for a and b (which won't be in our table).

       A possible implementation:
        
        // Cache initialisation
        int tableSize = 128 // Could make this larger even
        double KTCache[tableSize][tableSize];
        for (int i = 0; i < tableSize; i++) {
            for (int j = 0; j < tableSize; j++) {
                KTCache[i][j] = log( ((double) i + 0.5) / ((double) j + 1));
            }
        }

        // Later calls to logKTMul
        if (visits() < tableSize) {
            return KTCache[m_count[sym]][visits()];
        }

    */
}

void CTNode::updateLogProbWeighted() {
    // Compute log(0.5 * (P_e + P_w0 * P_w1))
    // == log(0.5) + log(P_e)
    //    + log(1 + exp(log(P_w0) + log(P_w1) - log(P_e))
    // TODO check syntax/calculation
    // Johannes: did check, seems ok to me

    double log_w0;
    if (m_child[false] != NULL) {
        log_w0 = m_child[false]->logProbWeighted();
    } else {
        log_w0 = 0; //set to zero if leaf node
    }

    double log_w1;
    if (m_child[true] != NULL) {
        log_w1 = m_child[true]->logProbWeighted();
    } else {
        log_w1 = 0; //set to zero if leaf node
    }

    errno = 0;
    double tmp_exp = exp(log_w0 + log_w1 - m_log_prob_est);
    if (errno == ERANGE) {
        // exp() overflowed the range of a double.
        // Then the '1 +' in '1 + exp' is irrelevant.
        // log(1 + exp(log(P_w0) + log(P_w1) - log(P_e)) becomes:
        // log(P_w0) + log(P_w1) - log(P_e)
        // Then log(P_e)'s cancel out.
        // TODO double check this. 
        // Johannes: looks good
        // log_half is defined static
        m_log_prob_weighted = log_half + log_w0 + log_w1; //approximate
    } else {
        m_log_prob_weighted = log_half + m_log_prob_est
            + log(1.0 + tmp_exp);
    }
}

// create a context tree of specified maximum depth
ContextTree::ContextTree(size_t depth) :
    m_root(new CTNode()),
    m_depth(depth)
{
    // Create a fictional history of 'depth' number of 0s.
    for (size_t i = 0; i < depth; ++i) {
        m_history.push_back(false);
    }
}


ContextTree::~ContextTree(void) {
    if (m_root) delete m_root;
}


// clear the entire context tree
void ContextTree::clear(void) {
    m_history.clear();
    // Create a fictional history of 'depth' number of 0s.
    for (size_t i = 0; i < m_depth; ++i) {
        m_history.push_back(false);
    }
    if (m_root) delete m_root;
    m_root = new CTNode();
}

// Update the CTW with the given symbol, and add that symbol to the history.
void ContextTree::update(symbol_t sym) {
    // TODO: implement
    
    // Path based on context
    // TODO consider renaming this, it's a bit long..
    CTNode *context_nodes[m_depth];

    context_nodes[0] = m_root;
    history_t::iterator hist_it = m_history.end() - 1;
    for (size_t n = 1; n < m_depth; ++n, --hist_it) {
        symbol_t context_symbol = *hist_it;//m_history[m_history.size() - n];
        // Create children as they are needed.
        if (context_nodes[n-1]->m_child[context_symbol] == NULL) {
            context_nodes[n-1]->m_child[context_symbol] = new CTNode();
        }
        context_nodes[n] = context_nodes[n-1]->m_child[context_symbol];
    }

    // Update possibilities from leaf back to root
    for (int n = m_depth - 1; n >= 0; --n) {
        // Update the log probabilities, after seeing sym.
        // Local KT estimate update, in log form.
        context_nodes[n]->m_log_prob_est = context_nodes[n]->m_log_prob_est
                                          + context_nodes[n]->logKTMul(sym);
        // Update a / b.
        ++(context_nodes[n]->m_count[sym]);

        // Update weighted probabilities
        if ((unsigned int) n == m_depth - 1) {
            // Leaf node
            context_nodes[n]->m_log_prob_weighted =
                    context_nodes[n]->logProbEstimated();
        } else {
            context_nodes[n]->updateLogProbWeighted();
        }
    }
    
    m_history.push_back(sym);
    
    /* Notes:
       Get context / CTNodes corresponding to that context
            - What to do when m_history is smaller than m_depth?
                Is a fictional history of 0s ok?
    Jesse:
    Rather than creating a node based on arbitrary histories, we could just
    not update (the probabilities, symbols still should be added to history)
    until we have enough context (seen depth bits).
    I'm not sure about this though.

       Create nodes if this is the first time we have seen this context.
       Update estimates back up to root - need to figure out weighting for log.
       Use -log of KT estimate, to avoid precision errors.
       Update KT estimate - something to do with logKTMul.

       Add "sym" to context.
        - m_history.push_back(sym);
    */
}


void ContextTree::update(const symbol_list_t &symlist) {
    // Call update, on each symbol in this list.
    for (symbol_list_t::const_iterator it = symlist.begin(); it != symlist.end(); ++it) {
        update(*it);
    }
}

// updates the history statistics, without touching the context tree
void ContextTree::updateHistory(const symbol_list_t &symlist) {
    for (size_t i=0; i < symlist.size(); i++) {
        m_history.push_back(symlist[i]);
    }
}


// removes the most recently observed symbol from the context tree
void ContextTree::revert(void) {
    // TODO: implement

    /* Notes:
       We can either store a snapshot of the tree in its entirety
            We probably won't have enough memory to do this.
       OR
       Just recompute the tree -
            delete whatever nodes were added by the last symbol.
            recalculate estimates.
    */
    
    // Get latest symbol (to update counts) and remove from history
    symbol_t latest_sym = m_history.back();
    m_history.pop_back();

    // Path based on context
    CTNode *context_nodes[m_depth];
    // context
    symbol_t context_symbols[m_depth];

    context_nodes[0] = m_root;
    // Traverse tree to leaf
    history_t::iterator hist_it = m_history.end() - 1;
    for (size_t n = 1; n < m_depth; ++n, --hist_it) {
        context_symbols[n] = *hist_it;//m_history[m_history.size() - n];
        context_nodes[n] = context_nodes[n-1]->m_child[context_symbols[n]];
    }

    // Update estimates
    for (int n = m_depth - 1; n >= 0; --n) {
        // Remove effects of last update
        --context_nodes[n]->m_count[latest_sym];

        // Delete node if it is no longer required
        if (context_nodes[n]->visits() == 0) {
            context_nodes[n-1]->m_child[context_symbols[n]] = NULL;
            delete context_nodes[n];
            continue;
        }

        context_nodes[n]->m_log_prob_est -= context_nodes[n]->logKTMul(latest_sym);

        // Update weighted probabilities
        if ((unsigned int) n == m_depth - 1) {
            // Leaf node
            context_nodes[n]->m_log_prob_weighted = context_nodes[n]->logProbEstimated();
        } else {
            context_nodes[n]->updateLogProbWeighted();
        }
    }
}

//revert n bits in
void ContextTree::revert(size_t bits){
    for (unsigned int i = 0; i < bits; ++i) {
        revert();
    }
}

//revert last bits in history without changing the ct
void ContextTree::revertHistory(size_t bits) {
    assert(bits <= m_history.size());
    for (unsigned int i = 0; i < bits; ++i) {
        m_history.pop_back();
    }
}



// generate a specified number of random symbols
// distributed according to the context tree statistics
void ContextTree::genRandomSymbols(symbol_list_t &symbols, size_t bits) {

    genRandomSymbolsAndUpdate(symbols, bits);

    // restore the context tree to it's original state
    for (size_t i=0; i < bits; i++) revert();
}


// generate a specified number of random symbols distributed according to
// the context tree statistics and update the context tree with the newly
// generated bits
void ContextTree::genRandomSymbolsAndUpdate(symbol_list_t &symbols, size_t bits) {
    for (size_t i=0; i < bits; i++) {
        //make a symbol somehow
        symbol_t sym; 

        double logJointProb = m_root->logProbWeighted();
        
        //add '0' to history, get probability
        update(false);
        double logJointWithSymbolProb = m_root->logProbWeighted();
        
        //calc probabilty that '0' follows
        double symbolCondProb = exp (logJointWithSymbolProb - logJointProb);
        
        sym = rand01() > symbolCondProb;
        
        // Only undo if necessary.
        if (sym) {
            revert();
            update(sym);
        }
        
        symbols.push_back(sym);
    }
}


// the logarithm of the block probability of the whole sequence
double ContextTree::logBlockProbability(void) {
    return m_root->logProbWeighted();
}

// TODO Watch out, returns the n'th history symbol (from the front),
//      not the n'th most recent (from the back).
// get the n'th most recent history symbol, NULL if doesn't exist
const symbol_t *ContextTree::nthHistorySymbol(size_t n) const {
    return n < m_history.size() ? &m_history[n] : NULL;
}

std::ostream& operator<< (std::ostream &out, CTNode &node){
    out << node.m_log_prob_est << " " << node.m_log_prob_weighted << " ";
    out << node.m_count[0] << " " << node.m_count[1] << " ";

    //output bit indicating that first child follows
    out << (node.m_child[0] != NULL) << " ";
    if(node.m_child[0] != NULL){
        out << (*node.m_child[0]);
    }

    //output bit indicating that second child follows
    out << (node.m_child[1] != NULL) << " ";
    if(node.m_child[1] != NULL){
        out << (*node.m_child[1]);
    }
    
    return out;
}

std::istream& operator>> (std::istream &in, CTNode &node){
    in >> node.m_log_prob_est;
    in >> node.m_log_prob_weighted;
    in >> node.m_count[0];
    in >> node.m_count[1];

    //output bit indicating that first child follows
    bool child_follows;
    in >> child_follows;
    if(child_follows){
        node.m_child[0] = new CTNode();
        in >> (*node.m_child[0]);
    }
    in >> child_follows;
    if(child_follows){
        node.m_child[1] = new CTNode();
        in >> (*node.m_child[1]);
    }
    
    return in;
}


// write context tree to stream
std::ostream& operator<< (std::ostream &out, ContextTree &ct){
    out << ct.m_depth << std::endl;
    for(history_t::iterator it = ct.m_history.begin(); it !=ct.m_history.end(); ++it){
        out << (*it);
    }
    out << std::endl;

    out << (*ct.m_root) << std::endl;
    
    return out;
}

//read context tree from stream
std::istream& operator>> (std::istream &in, ContextTree &ct){
    in >> ct.m_depth;
    
    in.get(); //read the next character out of the way
    char c = in.get();
    
    ct.m_history.clear();
    while(c != '\n'){
        ct.m_history.push_back(c == '1');
        c = in.get();
    }
    
    //read nodes recursivly
    in >> (*ct.m_root);
    
    return in;
}
