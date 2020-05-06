"""
Written by Gregor Urban <gur9000@outlook.com>

Copyright (C) 2020 Gregor Urban
Licensed under the Open Software License version 3.0
See COPYING or http://opensource.org/licenses/OSL-3.0
"""

import numpy as np
from os import makedirs as _makedirs
from os.path import exists as _exists

#####################
### Generic Functions
#####################

def mkdir(path):
    if len(path)>1 and _exists(path)==0:
        _makedirs(path)


def softmax(x):
    """Compute softmax values for each sets of scores in x. (numpy)
    """
    return np.exp(x) / (np.sum(np.exp(x), axis=1)[:, None] + 1e-16)


def binary_search(sorted_data, target):
    '''
    Returns index of match, if no perfect match is found then the index of the closest match is returned.
    '''
    lower = 0
    upper = len(sorted_data)
    while lower < upper:
        x = lower + (upper - lower) // 2
        val = sorted_data[x]
        if target == val:
            return x
        elif target > val:
            if lower == x:
                break
            lower = x
        elif target < val:
            upper = x
    if upper == len(sorted_data):
        return lower
    return lower if abs(sorted_data[lower] - target) <= abs(sorted_data[upper] - target) else upper



#########################
### MS-Specific Functions
#########################


def calcQCompetition_v2(predictions, labels):
    """Calculates P vs q xy points from arrays"""
    if labels.ndim==2:
        labels = np.argmax(labels, axis=1)
    if predictions.ndim==2:
        predictions = softmax(predictions)[:,1] #[:, 1] - predictions[:, 0]
    is_pos_neg = labels[np.argsort(predictions)[::-1]]
    ps = []
    fdrs = []
    posTot = 0.0
    fpTot = 0.0
    fdr = 0.0
    for ispos in is_pos_neg:
        if ispos == 1: posTot += 1.0
        else: fpTot += 1.0
        #--- check for zero positives
        if posTot == 0.0: fdr = 100.0
        else: fdr = fpTot / posTot
        #--- note the q
        fdrs.append(fdr)
        ps.append(posTot)
    qs = []
    lastQ = 100.0
    for idx in range(len(fdrs)-1, -1, -1):
        q = 0.0
        #--- q can never go up. 
        if lastQ < fdrs[idx]:
            q = lastQ
        else:
            q = fdrs[idx]
        lastQ = q
        qs.append(q)
    qs.reverse()
    return np.asarray(qs, 'float32'), np.asarray(ps, 'float32')


def AccuracyAtTol(predictions, labels, qTol=0.01):
    qs, ps = calcQCompetition_v2(predictions, labels)
    idx = binary_search(qs, qTol)
    return float(ps[idx]) / float(len(qs)) * 100


def AUC_up_to_tol(predictions, labels, qTol=0.005, qCurveCheck = 0.001):
    """
    Re-weighted AUC towards lower q values. Not normalized to 1.
    """
    if labels.ndim==2:
        labels = np.argmax(labels, axis=1)
    qs, ps = calcQCompetition_v2(predictions, labels)
    idx1 = binary_search(qs, qTol)
    idx2 = binary_search(qs, qCurveCheck)
    #den = float(np.sum(labels>=1))
    #print('AUC_upto_Tol: den =',den)
    auc = np.trapz(ps[:idx1])/len(ps)
    if qTol > qCurveCheck:
        auc = 0.3 * auc + 0.7 * np.trapz(ps[:idx2])/len(ps)
    return auc


