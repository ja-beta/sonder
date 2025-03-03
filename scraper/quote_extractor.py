import re


class QuoteExtractor:
    def __init__(self):
        self.QUOTE_MARKS = {
            '"': '"',    
            '“': '”',       
            "‘": "’",    
        }

        #'"'"’‘“”
    
    def extract_quotes(self, content):
        """Extract clean quotes using multiple quote marks"""
        valid_quotes = []
        
        for start_quote, end_quote in self.QUOTE_MARKS.items():
            start_escaped = re.escape(start_quote)
            end_escaped = re.escape(end_quote)
            pattern = f'{start_escaped}([^{end_escaped}]*){end_escaped}'
            quotes = re.findall(pattern, content)
            
            for quote in quotes:
                quote = quote.strip()
                if (len(quote) > 8 and  
                    len(quote) < 300 and 
                    not quote.startswith(('http', 'www', 'https'))):  
                    valid_quotes.append(quote)
        
        return valid_quotes
