/**
 * \file classifier.cpp
 * \author Kefei Lu
 * \brief Implementations of Classifier and related classes.
 */

#include "classifier.h"

#define PI 3.1415926
#define __CLASSIFICATION_DEBUG__

Classifier::Classifier( const Dataset& dataset,
	    const size_t classIndex,
	    const bool useAllAtt)
	    //const RSeed seed,
	    //const double tt_ratio)
{
    _bindedDataset = &dataset;
    _classIndex = classIndex;
    _useAllAtt = useAllAtt;
    //_seed = seed;
    _onlyTheseAtt.clear();
    perf_clear();

    empty_tt_set();

    //ran_tt_set();
}

void 
Classifier::init_tt_set(void)
{
    train_set().clear();
    test_set().clear();

    size_t nInst = dataset().num_of_inst();
    for( size_t i=0;i<nInst;i++ ) {
	train_set().push_back(i);
    }
    test_set() = train_set();
    return;
}

#ifdef ______XXXXXXXXXXXXX__________
/**
 * \deprecated 
 * This member is deprecated. It belongs to cross
 * validation, which should be a separated class
 */
void 
Classifier::ran_tt_set(void)
{
    fprintf(stdout, "(I) Randomizing T/T dataset...\n");

    train_set().clear();
    test_set().clear();

    size_t nInst = dataset().num_of_inst();

    size_t nTrain = nInst * tt_ratio();
    size_t nTest = nInst - nTrain;

    vector<short> used(nInst,0);
    size_t tmp=0;
    /* gen. training set */
    for (size_t i=0;i<nTrain;i++ ) {
	while (1) {
	    tmp = rand() % nTrain;
	    if (!used[tmp]) {
		train_set().push_back(tmp);
		used[tmp]++;
		break;
	    }
	}
    }
    // Sort into increasing order.
    sort( train_set().begin(), train_set().end() );

    /* the rest is testing set */
    for (size_t i=0;i<nInst;i++) {
	if (!used[i]) {
	    test_set().push_back(i);
	    used[i]++;
	}
    }

#ifdef __CLASSIFICATION_DEBUG__
    /* verify that every inst is used in either training
     * or testing set and is only used once. */
    assert(nTest==test_set().size());
    assert(nTrain==train_set().size());
    for (size_t i=0;i<nInst;i++) {
	assert(used[i]==1);
    }
#endif
}
#endif

void 
Classifier::test(void)
{
    assert(!test_set().empty());
    assert(!train_set().empty());

    conf().clear();
    size_t nTest = test_set().size();
    size_t nClass = dataset().get_att_desc( class_index() ).possible_value_vector().size();

    // Init. the Confusion Mat.
    {
	vector<double> tmp(nClass,.0);
	for( size_t i=0;i<nClass;i++ ) {
	    conf().push_back(tmp);
	}
	// Now it should be nClass x nClass matrix with zeros
    }

    // Init. the trust vector
    trust().clear();
    trust().resize(nClass, 0);

    // Begin testing
    for( size_t i=0;i<nTest;i++ ) {
	const Instance& inst = dataset()[test_set()[i]];
	Attribute klass = inst[class_index()];

	assert( !klass.unknown );

	NominalType est_c = classify_inst( inst );
	NominalType true_c = inst[class_index()].value.nom;
	conf()[est_c][true_c]++;
    }
    // Normalize the conf matrix
    // for each row:
    for( size_t r=0; r<nClass; r++ ) {
	// get row sum
	size_t sum = 0;
	for( size_t c=0; c<nClass; c++ ) {
	    sum += conf()[r][c];
	}
	// Normalize each column.
	for( size_t c=0; c<nClass; c++ ) {
	    conf()[r][c] /= sum;
	    if (c==r) {
		// this element is trust.
		trust()[c] = conf()[r][c];
	    }
	}
    }

    // print_performance();
}


NominalType 
StatisticsClassifier::classify_inst(const Instance& inst, double* maxProb)
{
    size_t nClass = 
	dataset().get_att_desc( class_index() ).possible_value_vector().size();
    size_t curMaxClassIndex = 0;
    double curMaxProb = -1;
    for( size_t i=0;i<nClass;i++ ) {
	double tmp = a_posteriori(i, inst);
	if ( tmp > curMaxProb ) {
	    curMaxProb = tmp;
	    curMaxClassIndex = i;
	}
    }

    if (maxProb)
	*maxProb = curMaxProb;

    return curMaxClassIndex;
}

double
NaiveBayesClassifier::
est_class_prob(const size_t c_index) const
{
    assert(!train_set().empty());
    assert(!test_set().empty());

    const AttDesc & classDesc = get_class_desc();
    size_t nClass = classDesc.possible_value_vector().size();
    assert(c_index<nClass);

    // count num of instance belongs to class i:
    const size_t nTrain = train_set().size();
    size_t sum = 0;
    // for each inst in train set
    for ( size_t j=0;j<nTrain;j++ ) {
	const Attribute& c = dataset()[train_set()[j]][this->class_index()];
	if (c.unknown) {continue;}
	if (c.value.nom == c_index) {sum ++;}
    }
    /** Handling zero-instance issue (no inst. belongs to this class). */
    if (sum==0) {
	fprintf(stderr, "(W) No Training instance belongs to class %s (%d).\n",
		get_class_desc().map(c_index).c_str(), c_index);
    }
    return sum/nTrain;
}

double 
NaiveBayesClassifier::
est_att_prob_on_class(const ValueType& value, const size_t att_i, const size_t class_j) const
{
    // Check if training/testing set has been specified before.
    assert(!train_set().empty());
    assert(!test_set().empty());

    // Check if train() has been called before.
    assert(!pClass().empty());

    return _attDistrOnClass.prob(value, att_i, class_j);
}

void
NaiveBayesClassifier::
train(void)
{
    fprintf(stdout, "(I) Training the model...\n");

    assert(!train_set().empty());
    assert(!test_set().empty());

    pClass().clear();
    //distrAttOnClass().clear();

    // Obtaining _pClass:
    size_t nClass = get_class_desc().possible_value_vector().size();
    for ( size_t i=0;i<nClass;i++ ) {
	pClass().push_back( est_class_prob(i) );
    }

    const size_t nAtt = dataset().num_of_att();
    const size_t ci = class_index();
    // Obtaining _attDistrOnClass:
    for ( size_t i=0; i<nAtt; i++ ) {
	if ( i == ci ) continue;
	for ( size_t j=0; j<nClass; j++ ) {
	    calc_distr_for_att_on_class(i,j);
	    //Distribution *& pDistr = attDistrOnClass().table()[j][i];
	    //pDistr = calc_distr_for_att_on_class(i,j);
	}
    }
    // for ( size_t i=0; i<nAtt; i++ ) {
    //     if ( i == ci ) continue;
    //     const AttDesc& desc = dataset().get_att_desc(i);
    //     if (desc.get_type() ==  ATT_TYPE_NUMERIC) {
    //         /* Temp struct, used to collect statistics to evaluate the 
    //          * Gaussian distributions. */
    //         struct Tmp {
    //     	double sum;
    //     	double sq_sum; // squared sum
    //     	size_t N; // num of inst belongs to a class
    //         };
    //         vector<Tmp> sta(nClass);
    //         // Scan all the instances, 
    //         // collect statistics for P(A_i|C_k), for all k.
    //         for ( size_t j=0; j<nInst; j++ ) {
    //     	// class unknown instance, don't count
    //     	if ( dataset()[j][ci].unknown ) continue;
    //     	if ( dataset()[j][i].unknown ) continue;
    //     	NominalType klass = dataset()[j][ci].value.nom;
    //     	sta[klass].sum += dataset()[j][i].value.num;
    //     	sta[klass].sq_sum += pow(dataset()[j][i].value.num,2);
    //     	sta[klass].N ++;
    //         }
    //         // Calculate mean and var for A_i|C_k for all k, 
    //         // put mean and var of A_i|C_k, for all k, into 
    //         // attDistrOnClass table column i
    //         for (size_t k=0; k<nClass; k++) {
    //     	Distribution*& pDistr = attDistrOnClass().table()[k][i];
    //     	pDistr = new NormalDistribution;
    //     	pDistr->mean() = sta[k].sum / sta[k].N;
    //     	pDistr->var() = sta[k].sq_sum / (sta[k].N - 1);
    //         }
    //     }
    //     else if (desc.get_type() == ATT_TYPE_NOMINAL) {
    //         struct Tmp {
    //     	// num of positive instance
    //     	size_t nPosInstInClassK;
    //     	size_t nClassK;
    //         };
    //     }
    //     else {
    //         fprintf(stderr, "(E) Unsupported attribute type: %s(%d).\n",
    //     	    desc.map(desc.get_type()).c_str(),desc.get_type());
    //         exit(1);
    //     }
    // }

    //for ( size_t i=0;i<nClass;i++ ) {
    //    for (size_t j=0;j<nAtt;j++ ) {
    //        if ( j == ci ) continue;

    //        // j is garranteed an attribute, not the class
    //        const AttDesc& desc = dataset().get_att_desc(j);
    //        if ( desc.get_type() == ATT_TYPE_NUMERIC ) {
    //    	size_t sum = 0; 
    //    	size_t N = 0; // num of inst belongs to this class
    //    	size_t sqr_sum = 0;
    //    	for (size_t k=0;k<nInst;k++) {

    //    	}
    //        }
    //        else if (desc.get_type() == ATT_TYPE_NOMINAL) {
    //        }
    //        else {
    //    	fprintf(stderr, "(E) Unsupported attribute type: %s(%d).\n",
    //    		desc.map(desc.get_type()).c_str(),desc.get_type());
    //    	exit(1);
    //        }
    //    }
    //}
}

//Distribution* 
void
NaiveBayesClassifier::
calc_distr_for_att_on_class(size_t att_i, size_t class_j)
{
    const Dataset& ds = dataset();
    const size_t nInst = ds.num_of_inst();
    const size_t ci = class_index();
    const size_t nClass = ds.get_att_desc(ci).possible_value_vector().size();

    // att_i is not the class attribute:
    if (att_i == ci) {
	fprintf(stderr, "(E) Attribute must not be the class attribute.\n");
	exit(1);
    }

    // This value to later store the new'd distribution 
    // and will be returned.
    //Distribution* pDistr = NULL;
    Distribution*& pDistr = attDistrOnClass().table()[class_j][att_i];
    // find type of this att:
    const AttDesc& desc = ds.get_att_desc(att_i);
    if (desc.get_type() == ATT_TYPE_NUMERIC) {
	//pDistr = new NormalDistribution;
	double sum=0;
	double sq_sum=0;
	size_t nInstBelongsToThisClass=0;
	for (size_t i=0;i<nInst;i++) {
	    const Attribute& klass = ds[i][ci];
	    const Attribute& att = ds[i][att_i];
	    if (klass.unknown) continue;
	    if (att.unknown) continue;
	    if (klass.value.nom != class_j) continue;
	    sum += att.value.num;
	    sq_sum += pow(att.value.num,2);
	    nInstBelongsToThisClass ++;
	}
	if (nInstBelongsToThisClass==0) {
	    fprintf(stderr, "(E) No instances belongs to class:%s(%d).\n",
		    desc.map(desc.get_type()).c_str(),desc.get_type());
	    exit(1);
	}
	((NormalDistribution*)pDistr)->mean() = sum/nInstBelongsToThisClass;
	((NormalDistribution*)pDistr)->var() = sq_sum/(nInstBelongsToThisClass-1);
    }
    else if (desc.get_type() == ATT_TYPE_NOMINAL) {
	//pDistr = new NominalDistribution;
	((NominalDistribution*)pDistr)->pmf().resize(nClass,0.0);
	size_t sum = 0;
	for (size_t i=0;i<nInst;i++) {
	    const Attribute& klass = ds[i][ci];
	    const Attribute& att = ds[i][att_i];
	    if (klass.unknown) continue;
	    if (att.unknown) continue;
	    if (klass.value.nom != class_j) continue;
	    sum ++;
	    ((NominalDistribution*)pDistr)->pmf()[att.value.nom] ++;
	}
	// Handle zero sum issue and so on.
	if (sum==0) {
	    fprintf(stderr, "(E) No instances belongs to class:%s(%d).\n",
		    desc.map(desc.get_type()).c_str(),desc.get_type());
	    exit(1);
	}
	bool zero_issue=0;
	for (size_t i=0;i<nClass;i++) {
	    size_t n = ((NominalDistribution*)pDistr)->pmf()[i];
	    if (n==0) {
		zero_issue = 1;
	    }
	}
	/**
	 * Handle the zero possibility issue.
	 *
	 * p{A_j|C_i} = N(A_j,C_i) / N(C_i)
	 *
	 * if N(A_j,C_i) == 0, means there's no such instance
	 * having A_j value and belongs to class C_i.
	 * This can be handled as:
	 *
	 * \verbatim
	                N(A_j,C_i) + 1
	 p{A_j|C_i} = ----------------- ,
	                N(C_i) + nPos
	   \endverbatim
	 *
	 * where nPos is the num of possible values of this 
	 * attribute.
	 *
	 * For example, 0/3, 3/3 will become 1/5, 4/5; 
	 * 0/3, 1/3, 2/3 will become 1/6, 2/6, 3/6.
	 */
	size_t nPos = ds.get_att_desc(att_i).possible_value_vector().size();
	for (size_t i=0;i<nClass;i++) {
	    if (!zero_issue) {
		((NominalDistribution*)pDistr)->pmf()[i] /= sum;
	    } else {
		((NominalDistribution*)pDistr)->pmf()[i] = (((NominalDistribution*)pDistr)->pmf()[i] + 1) / (sum + nPos);
	    }
	}
    }
    fprintf(stderr, "(E) Unsupported attribute type: %s(%d).\n",
	    desc.map(desc.get_type()).c_str(),desc.get_type());
    exit(1);
}

double 
NaiveBayesClassifier::
a_posteriori(const NominalType c, const Instance& inst)
{
    return 0;
}

void 
NaiveBayesClassifier::
bind_dataset(const Dataset& dataset)
{
    Classifier::bind_dataset(dataset);
    attDistrOnClass().init_table();
}

void
AttDistrOnClass::
init_table(void)
{
    if (!_classifier) {
	fprintf(stderr, "(E) No binding classifier.\n");
	exit(1);
    }

    const Classifier & c = classifier();
    size_t nAtt = c.dataset().num_of_att() - 1;
    size_t cIndex = c.class_index();
    size_t nClass = c.get_class_desc().possible_value_vector().size();

    {
	vector<Distribution*> distr;
	Distribution* tmp;
	for (size_t i=0;i<nAtt+1;i++) {
	    if (i==cIndex) continue;
	    const AttDesc& desc = c.dataset().get_att_desc(i);
	    if (desc.get_type() == ATT_TYPE_NUMERIC) {
		tmp = new NormalDistribution;
		distr.push_back(tmp);
	    }
	    else if (desc.get_type() == ATT_TYPE_NOMINAL) {
		tmp = new NominalDistribution;
		distr.push_back(tmp);
	    }
	    else {
		fprintf(stderr, "(E) Unsupported type: %s (%d).\n",
			desc.map(desc.get_type()).c_str(), desc.get_type());
	    }
	}
	for (size_t i=0;i<nClass;i++) {
	    table().push_back(distr);
	}
    }
}

AttDistrOnClass::
~AttDistrOnClass()
{
    // Free all pointers
    if (_table.empty()) return;

    size_t nRow = _table.size();
    for (size_t i=0;i<nRow;i++) {
	for (size_t j=0;j<_table[i].size();j++) {
	    if (_table[i][j]) {
		delete _table[i][j];
		_table[i][j] = NULL;
	    }
	}
    }

    _table.clear();
    return;
}

const double 
NormalDistribution::
prob(const ValueType value) const
{
    return (1/sqrt(2*PI*var())) * exp( - pow(value.num-mean(),2) / (2*var()) );
}
