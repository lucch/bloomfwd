import Control.Applicative
import Control.Monad
import Data.Array.IO
import Data.List
import System.IO
import System.Random

pfxsFile = "/Users/alexandrelucchesi/Development/c/bloomfwd/data/as6447_prefixes.txt"

numAddrsPerPrefixFile = "/Users/alexandrelucchesi/Development/c/bloomfwd/data/matchingAddrs-count.txt"

destFile = "/Users/alexandrelucchesi/Development/c/bloomfwd/data/matchingAddrs.txt"

main = do
    addrs <- generateAddrs >>= shuffle
    putStrLn $ "Writing " ++ show (length addrs) ++ " addresses to file..."
    h <- openFile destFile WriteMode
    hPutStrLn h $ show (length addrs)
    hPutStr h $ unlines addrs 
    hClose h -- Extremely important to close the file so that contents are completely written.

generateAddrs :: IO [String]
generateAddrs = liftM concat $ forM [1..32] $ \i -> do
    s <- readFile pfxsFile
    ns <- map read <$> numAddrs
    if length ns /= 32 || sum ns /= 2^20
        then error "Something is wrong!"
        else return ()
    let pfxs  = filter (("/" ++ show i) `isInfixOf`) . lines $ s
        numPfxs = ns !! (i - 1)
        addrs = map (takeWhile (/= '/')) pfxs
    if numPfxs > 0 && null addrs
        then do
            putStrLn $ "There is no prefix of size: /" ++ show i ++ " (needed)!"
            return []
        else
            return $ take numPfxs (cycle addrs)
  where
    numAddrs = lines <$> readFile numAddrsPerPrefixFile

-- | Randomly shuffle a list
--   /O(N)/
shuffle :: [a] -> IO [a]
shuffle xs = do
        ar <- newArray n xs
        forM [1..n] $ \i -> do
            j <- randomRIO (i,n)
            vi <- readArray ar i
            vj <- readArray ar j
            writeArray ar j vi
            return vj
  where
    n = length xs
    newArray :: Int -> [a] -> IO (IOArray Int a)
    newArray n xs =  newListArray (1,n) xs

