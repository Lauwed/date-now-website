<script>
	import Card from './Card.svelte';
	import Socials from './Socials.svelte';
</script>

<Card tag="footer" customClass="footer">
	<Socials />

	<ul>
		<li><a href="/legal-notice">Legal Notice</a></li>
	</ul>

	<p class="footer__copy">Date.now() 2026 - <a href="www.lauradurieux.dev">Laura durieux</a></p>
</Card>

<style lang="scss">
	:global(.footer) {
		display: flex;
		flex-direction: column;
		gap: 16px;
	}

	.footer {
		&__copy {
			margin: 0;
			font-size: 0.75rem;
		}
	}
</style>
