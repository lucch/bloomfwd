{-# LANGUAGE CPP #-}

module IPv6LookupOpt where

import Data.Map (fromList, toList, updateAt)
import Data.Maybe (fromJust, fromMaybe)
import Data.List (elemIndex, findIndices, sort, subsequences)
#ifdef DEBUG
import Debug.Trace
#endif

type Histogram = [(Int, Int)]

sampleHistogram :: Histogram
sampleHistogram =
    [ (1, 1)
    , (2, 2)
    , (3, 0)
    , (4, 1)
    , (5, 1) ]

histogramFromDistrib :: FilePath -> IO Histogram
histogramFromDistrib path = do
    s <- readFile path
    return $ fmap (tupleToInt . break (== ' ')) $ lines s
  where
    tupleToInt (x, y) = (read x, read y)

nonEmptyLengths :: Histogram -> [Int]
nonEmptyLengths = map fst . filter ((/= 0) . snd)

prefixesCount :: Histogram -> Int
prefixesCount = sum . map snd

-- | Optimally using SIMD registers requires `prefixKeysLen = distinctLengths *
-- len` to be a multiple of sixteen.
--
simdOptimal :: Int    -- ^ Number of distinct prefixes length (excluding the default route)
            -> [Int]  -- ^ Infinite list holding the possible values of `len`
simdOptimal distinctLengths =
    [numAddrsIter | numAddrsIter <- [1..], numAddrsIter * distinctLengths `rem` 16 == 0]

type Indices = [Int]

cost :: Histogram -> Indices -> Integer
cost hist indices =
    let hists = splitByIndices indices hist
    in sum $ map histCost hists
  where
    histCost :: Histogram -> Integer
    histCost hist = sum $ map (\(i, q) -> 2^(maxIndex - i) * fromIntegral q) hist
      where
        maxIndex = fst (last hist)

splitByIndices :: [Int] -> [a] -> [[a]]
splitByIndices [] lst = [lst]
splitByIndices (x:xs) lst =
    let (first, rest) = splitAt x lst
    in first : (splitByIndices (map (\y -> y - x)  xs) rest)

cheaper :: Histogram -> Int -> Indices
cheaper hist numIndices = solve hist [firstChoice] (numIndices - 1)
  where
    firstChoice = fst (last hist)

    solve hist indices n 
        | n > 0 =
            let candidates = map sort
                  [i : indices | i <- [1..firstChoice], i `notElem` indices]
                costs = map (cost hist) candidates
                minCostIndex = fromJust $ elemIndex (minimum costs) costs
            in
#ifdef DEBUG
                trace (show $ zip costs candidates) $
#endif
                solve hist (candidates !! minCostIndex) (n - 1)
        | otherwise = indices

cheaperBruteForce :: Histogram -> Int -> [(Integer, Indices)]
cheaperBruteForce hist numIndices
    | numIndices == 0 = [(cost hist [firstChoice], [firstChoice])]
    | otherwise =
        let indices = [ xs | xs <- subsequences [1..firstChoice]
                           , (not . null) xs
                           , last xs == firstChoice
                           , length xs <= numIndices ]
            costsIndices = map (\is -> (cost hist is, is)) indices 
        in filter (\t -> fst t == fst (minimum costsIndices)) costsIndices
  where
    firstChoice = fst (last hist)

