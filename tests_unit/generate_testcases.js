const path = require("path");
const fs = require("fs");
const testCasesFunction = require('tests-common')

function getTestCases() {
    const casesFunction = Object.keys(testCasesFunction);
    const cases = []
    for (const rawCase of casesFunction) {
        cases.push({
            caseName: rawCase,
            txFunction: testCasesFunction[rawCase]
        });
    }
    return cases;
}


function main() {
    for (const testCase of getTestCases()) {
        const outputPath = path.join(__dirname, "testcases", `${testCase.caseName}.raw`)
        const buf = testCasesFunction.feeBumpTx().signatureBase()
        fs.writeFile(outputPath, buf, (err) => {
            if (err) {
                console.log(`Failed to write to ${testCase.caseName}`)
                console.log(err)
            }
        });
    }
    console.log("Finished.")
}


main()

