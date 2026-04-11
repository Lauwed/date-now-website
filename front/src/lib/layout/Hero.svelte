<script>
	import Button from '$lib/components/Button.svelte';
	import Card from '$lib/components/Card.svelte';
	import Logo from '$lib/components/Logo.svelte';
	import VODsButton from '$lib/components/VODsButton.svelte';
</script>

<Card customClass="hero" tag="section">
	<h1 class="hero__title"><Logo size="lg" /></h1>

	<div>
		<p class="hero__welcome">
			Welcome to Date.now(), your go-to source for everything tech and development.
		</p>

		<p>
			We cover cybersecurity, web and software development, AI ethics, and much more — including the
			remarkable women who shaped the history of computer science.
		</p>
	</div>

	<div class="hero__buttons">
		<Button variant="primary">Subscribe to the newsletter!</Button>
		<Button tag="a" href="/used-newsletters">List of the newsletters I use</Button>
		<VODsButton />
	</div>
</Card>

<style lang="scss">
	@use '../styles/mixins.scss' as *;

	:global(.hero) {
		display: flex;
		flex-direction: column;
		gap: 12px;

		p {
			margin: 0;
		}
	}

	.hero {
		&__title {
			width: fit-content;
			margin: 0 auto;
			line-height: 1;
			height: 75px;

			@include md {
				margin: 0;
			}
		}

		&__welcome {
			font-size: 1.175rem;
			padding-bottom: 4px;
		}

		&__buttons {
			display: flex;
			flex-wrap: wrap;
			justify-content: center;
			gap: 12px;
			margin: 10px 0 0;

			@include md {
				justify-content: start;
			}
		}
	}
</style>
